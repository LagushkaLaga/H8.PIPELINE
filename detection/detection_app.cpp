#include "detection_app.hpp"

#include <fstream>
#include <ctime>
#include <unistd.h>

std::string gen_random(unsigned len) {
  static const char alphanum[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
  std::string tmp_s;
  tmp_s.reserve(len);

  for (unsigned i = 0; i < len; ++i) {
    tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
  }

  return tmp_s;
}

hailo_status create_feature(hailo_output_vstream vstream, std::shared_ptr< FeatureData > & feature)
{
  hailo_vstream_info_t vstream_info = {};
  auto status = hailo_get_output_vstream_info(vstream, & vstream_info);
  if (HAILO_SUCCESS != status)
  {
    std::cerr << "Failed to get output vstream info with status = " << status << std::endl;
    return status;
  }

  size_t output_frame_size = 0;
  status = hailo_get_output_vstream_frame_size(vstream, & output_frame_size);
  if (HAILO_SUCCESS != status)
  {
    std::cerr << "Failed getting output virtual stream frame size with status = " << status << std::endl;
    return status;
  }

  feature = std::make_shared< FeatureData >(
      static_cast<uint32_t>(output_frame_size),
      vstream_info.quant_info.qp_zp,
      vstream_info.quant_info.qp_scale,
      vstream_info.shape.width,
      vstream_info
    );

  return HAILO_SUCCESS;
}

hailo_status dump_detected_object(const HailoDetectionPtr & detection, std::ofstream & detections_file)
{
  if (detections_file.fail())
  {
    return HAILO_FILE_OPERATION_FAILURE;
  }

  HailoBBox bbox = detection->get_bbox();
  detections_file << "Detection object name:          " << detection->get_label() << "\n";
  detections_file << "Detection object id:            " << detection->get_class_id() << "\n";
  detections_file << "Detection object confidence:    " << detection->get_confidence() << "\n";
  detections_file << "Detection object Xmax:          " << bbox.xmax() * YOLOV5M_IMAGE_WIDTH << "\n";
  detections_file << "Detection object Xmin:          " << bbox.xmin() * YOLOV5M_IMAGE_WIDTH << "\n";
  detections_file << "Detection object Ymax:          " << bbox.ymax() * YOLOV5M_IMAGE_HEIGHT << "\n";
  detections_file << "Detection object Ymin:          " << bbox.ymin() * YOLOV5M_IMAGE_HEIGHT << "\n"
                  << std::endl;

  return HAILO_SUCCESS;
}

hailo_status post_processing_all(std::vector< std::shared_ptr< FeatureData > > & features, const size_t frames_count, std::vector< HailoRGBMat > & input_images)
{
  auto status = HAILO_SUCCESS;

  std::sort(features.begin(), features.end(), & FeatureData::sort_tensors_by_size);
  for (size_t i = 0; i < frames_count; i++)
  {
    HailoROIPtr roi = std::make_shared< HailoROI >(HailoROI(HailoBBox(0.0f, 0.0f, 1.0f, 1.0f)));
    for (uint j = 0; j < features.size(); j++)
    {
      roi->add_tensor(std::make_shared< HailoTensor >(reinterpret_cast< uint8_t * >(features[j]->m_buffers.get_read_buffer().data()), features[j]->m_vstream_info));
    }

    yolov5(roi);

    for (auto & feature : features)
    {
      feature->m_buffers.release_read_buffer();
    }

    //status = write_txt_file(roi, input_images[i].get_name());

    status = write_image(input_images[i], roi);
  }
  return status;
}

hailo_status write_image(HailoRGBMat & image, HailoROIPtr roi)
{
  std::string file_name = image.get_name();
  auto draw_status = draw_all(image, roi, true);
  if (OVERLAY_STATUS_OK != draw_status)
  {
    std::cerr << "Failed drawing detections on image '" << file_name << "'. Got status " << draw_status << "\n";
  }
  
  std::vector< HailoDetectionPtr > detections = hailo_common::get_hailo_detections(roi);
  std::cout << file_name << "-" << detections[0]->get_label();
  cv::Mat write_mat;
  cv::cvtColor(image.get_mat(), write_mat, cv::COLOR_RGB2BGR);
  auto write_status = cv::imwrite("output_images/" + file_name + "/" + gen_random(20) + ".bmp", write_mat);
  return HAILO_SUCCESS;
}

hailo_status write_txt_file(HailoROIPtr roi, std::string file_name)
{
  std::vector< HailoDetectionPtr > detections = hailo_common::get_hailo_detections(roi);
  hailo_status status = HAILO_SUCCESS;

  if (detections.size() == 0)
  {
    std::cout << "No detections were found in file '" << file_name << "'\n";
    return status;
  }

  auto detections_file = "output_images/" + file_name + "_detections.txt";
  std::ofstream ofs(detections_file, std::ios::out);
  if (ofs.fail())
  {
    std::cerr << "Failed opening output file: '" << detections_file << "'\n";
    return HAILO_OPEN_FILE_FAILURE;
  }

  for (auto & detection : detections)
  {
    if (0 == detection->get_confidence())
    {
      continue;
    }
    status = dump_detected_object(detection, ofs);
    if (HAILO_SUCCESS != status)
    {
      std::cerr << "Failed dumping detected object in output file: '" << detections_file << "'\n";
    }
  }
  return status;
}

hailo_status write_all(hailo_input_vstream input_vstream, std::vector< HailoRGBMat > & input_images)
{
  for (auto & input_image : input_images)
  {
    auto image_mat = input_image.get_mat();
    hailo_status status = hailo_vstream_write_raw_buffer(input_vstream, image_mat.data, image_mat.total() * image_mat.elemSize());
    if (HAILO_SUCCESS != status)
    {
      std::cerr << "Failed writing to device data of image '" << input_image.get_name() << "'. Got status = " << status << std::endl;
      return status;
    }
  }
  return HAILO_SUCCESS;
}

hailo_status read_all(hailo_output_vstream output_vstream, const size_t frames_count, std::shared_ptr< FeatureData > feature)
{
  for (size_t i = 0; i < frames_count; i++)
  {
    auto &buffer = feature->m_buffers.get_write_buffer();
    hailo_status status = hailo_vstream_read_raw_buffer(output_vstream, buffer.data(), buffer.size());
    feature->m_buffers.release_write_buffer();

    if (HAILO_SUCCESS != status)
    {
      std::cerr << "Failed reading with status = " << status << std::endl;
      return status;
    }
  }
  return HAILO_SUCCESS;
}

hailo_status run_inference_threads(
    hailo_input_vstream input_vstream,
    hailo_output_vstream * output_vstreams,
    const size_t output_vstreams_size,
    std::vector< HailoRGBMat > & input_images
  )
{
  std::vector< std::shared_ptr< FeatureData > > features;

  features.reserve(output_vstreams_size);
  for (size_t i = 0; i < output_vstreams_size; i++)
  {
    std::shared_ptr< FeatureData > feature(nullptr);
    auto status = create_feature(output_vstreams[i], feature);
    if (HAILO_SUCCESS != status)
    {
      std::cerr << "Failed creating feature with status = " << status << std::endl;
      return status;
    }
    features.emplace_back(feature);
  }

  const size_t frames_count = input_images.size();
  std::vector< std::future< hailo_status > > output_threads;
  output_threads.reserve(output_vstreams_size);
  for (size_t i = 0; i < output_vstreams_size; i++)
  {
    output_threads.emplace_back(std::async(read_all, output_vstreams[i], frames_count, features[i]));
  }

  auto input_thread(std::async(write_all, input_vstream, std::ref(input_images)));
  auto pp_thread(std::async(post_processing_all, std::ref(features), frames_count, std::ref(input_images)));

  hailo_status out_status = HAILO_SUCCESS;
  for (size_t i = 0; i < output_threads.size(); i++)
  {
    auto status = output_threads[i].get();
    if (HAILO_SUCCESS != status)
    {
      out_status = status;
    }
  }
  auto input_status = input_thread.get();
  auto pp_status = pp_thread.get();

  if (HAILO_SUCCESS != input_status)
  {
    std::cerr << "Write thread failed with status " << input_status << std::endl;
    return input_status;
  }
  if (HAILO_SUCCESS != out_status)
  {
    std::cerr << "Read failed with status " << out_status << std::endl;
    return out_status;
  }
  if (HAILO_SUCCESS != pp_status)
  {
    std::cerr << "Post-processing failed with status " << pp_status << std::endl;
    return pp_status;
  }

  std::cout << "Inference finished successfully" << std::endl;

  return HAILO_SUCCESS;
}

hailo_status custom_infer(std::vector< HailoRGBMat > & input_images)
{
  hailo_status status = HAILO_UNINITIALIZED;
  hailo_device device = NULL;
  hailo_hef hef = NULL;
  hailo_configure_params_t config_params = {0};
  hailo_configured_network_group network_group = NULL;
  size_t network_group_size = 1;
  hailo_input_vstream_params_by_name_t input_vstream_params[INPUT_COUNT] = {0};
  hailo_output_vstream_params_by_name_t output_vstream_params[OUTPUT_COUNT] = {0};
  size_t input_vstreams_size = INPUT_COUNT;
  size_t output_vstreams_size = OUTPUT_COUNT;
  hailo_activated_network_group activated_network_group = NULL;
  hailo_input_vstream input_vstreams[INPUT_COUNT] = {NULL};
  hailo_output_vstream output_vstreams[OUTPUT_COUNT] = {NULL};

  status = hailo_create_pcie_device(NULL, & device);
  REQUIRE_SUCCESS(status, l_exit, "Failed to create pcie_device");

  status = hailo_create_hef_file(& hef, HEF_FILE);
  REQUIRE_SUCCESS(status, l_release_device, "Failed reading hef file");

  status = hailo_init_configure_params(hef, HAILO_STREAM_INTERFACE_PCIE, & config_params);
  REQUIRE_SUCCESS(status, l_release_hef, "Failed initializing configure parameters");

  status = hailo_configure_device(device, hef, &config_params, & network_group, & network_group_size);
  REQUIRE_SUCCESS(status, l_release_hef, "Failed configure devcie from hef");
  REQUIRE_ACTION(network_group_size == 1, status = HAILO_INVALID_ARGUMENT, l_release_hef, "Invalid network group size");

  status = hailo_make_input_vstream_params(
      network_group,
      true,
      HAILO_FORMAT_TYPE_AUTO,
      input_vstream_params,
      & input_vstreams_size
    );

  REQUIRE_SUCCESS(status, l_release_hef, "Failed making input virtual stream params");

  status = hailo_make_output_vstream_params(
      network_group,
      true,
      HAILO_FORMAT_TYPE_AUTO,
      output_vstream_params,
      & output_vstreams_size
    );

  REQUIRE_SUCCESS(status, l_release_hef, "Failed making output virtual stream params");

  REQUIRE_ACTION(
      ((input_vstreams_size == INPUT_COUNT) || (output_vstreams_size == OUTPUT_COUNT)),
      status = HAILO_INVALID_OPERATION,
      l_release_hef,
      "Expected one input vstream and three outputs vstreams"
    );

  status = hailo_create_input_vstreams(network_group, input_vstream_params, input_vstreams_size, input_vstreams);
  REQUIRE_SUCCESS(status, l_release_hef, "Failed creating input virtual streams");

  status = hailo_create_output_vstreams(network_group, output_vstream_params, output_vstreams_size, output_vstreams);
  REQUIRE_SUCCESS(status, l_release_input_vstream, "Failed creating output virtual streams");

  status = hailo_activate_network_group(network_group, NULL, & activated_network_group);
  REQUIRE_SUCCESS(status, l_release_output_vstream, "Failed activating network group");

  status = run_inference_threads(input_vstreams[0], output_vstreams, output_vstreams_size, input_images);
  REQUIRE_SUCCESS(status, l_deactivate_network_group, "Inference failure");

  status = HAILO_SUCCESS;
  l_deactivate_network_group : (void)hailo_deactivate_network_group(activated_network_group);
  l_release_output_vstream : (void)hailo_release_output_vstreams(output_vstreams, output_vstreams_size);
  l_release_input_vstream : (void)hailo_release_input_vstreams(input_vstreams, input_vstreams_size);
  l_release_hef : (void)hailo_release_hef(hef);
  l_release_device : (void)hailo_release_device(device);
  l_exit : return status;
}

Camera::Camera(std::string name, std::string url):
  cam_(url),
  name_(name)
{}

Camera::~Camera()
{
  cam_.release();
}

cv::Mat Camera::read_frame()
{
  cv::Mat temp;
  cam_.read(temp);
  return temp;
}

std::string & Camera::get_name() noexcept
{
  return name_;
}

bool Camera::is_open() const noexcept
{
  return cam_.isOpened();
}

std::vector< Camera > read_rtps(std::istream & in)
{
  std::vector< Camera > result;
  while (in)
  {
    std::string name, url;
    in >> name >> url;
    Camera temp(name, url);
    if (!in) break;
    result.push_back(temp);
  }
  return result;
}

std::vector< HailoRGBMat > read_frames(std::vector< Camera > & source)
{
  std::vector< HailoRGBMat > result;
  for (auto ins : source)
  {
    std::string file_name = ins.get_name();
    cv::Mat bgr_mat = ins.read_frame();
    cv::Mat rgb_mat;
    cv::cvtColor(bgr_mat, rgb_mat, cv::COLOR_BGR2RGB);
    HailoRGBMat image = HailoRGBMat(rgb_mat, file_name);
    result.push_back(image);
  }
  return result;
}

int main()
{
  std::ifstream rtps_file;
  rtps_file.open("rtps.txt");
  std::vector< Camera > rtps_cams = read_rtps(rtps_file);
  rtps_file.close();

  auto status = HAILO_SUCCESS;
  while (status == HAILO_SUCCESS)
  {
    std::vector< HailoRGBMat > input_frames = read_frames(rtps_cams);
    status = custom_infer(input_frames);
  }

  return HAILO_SUCCESS;
}
