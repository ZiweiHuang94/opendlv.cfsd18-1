/**
* Copyright (C) 2017 Chalmers Revere
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

#include <iostream>

#include <opendavinci/odcore/data/TimeStamp.h>
#include <opendavinci/odcore/strings/StringToolbox.h>
#include <opendavinci/odcore/wrapper/Eigen.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d/calib3d.hpp>

#include "detectcone.hpp"

namespace opendlv {
namespace logic {
namespace cfsd18 {
namespace perception {

DetectCone::DetectCone(int32_t const &a_argc, char **a_argv) :
  DataTriggeredConferenceClientModule(a_argc, a_argv, "logic-cfsd18-perception-detectcone")
, m_img()
{
}

DetectCone::~DetectCone()
{
  m_img.release();
}

void DetectCone::setUp()
{
  //rectify();
  slidingWindow("sliding_window", "0.png");
  /*
  tiny_dnn::vec_t data;
  std::string img_path;
  for( int a = 0; a < 2; a = a + 1 ) {
      img_path = "data/yellow/" + std::to_string(a) + ".png";
      convert_image(img_path, 0, 1, 25, 25, data);
   }*/

}

void DetectCone::tearDown()
{
}

void DetectCone::nextContainer(odcore::data::Container &a_container)
{
  odcore::data::TimeStamp incommingDataTime = a_container.getSampleTimeStamp();
  double currentTime = static_cast<double>(incommingDataTime.toMicroseconds())/1000000.0;
  std::cout << "Current time: " << currentTime << "s" << std::endl;

  if (a_container.getDataType() == odcore::data::image::SharedImage::ID()) {
    odcore::data::image::SharedImage sharedImg =
        a_container.getData<odcore::data::image::SharedImage>();
    if (!ExtractSharedImage(&sharedImg)) {
      std::cout << "[" << getName() << "] " << "[Unable to extract shared image."
          << std::endl;
      return;
    }

    // saveImg(currentTime);
  }

}

bool DetectCone::ExtractSharedImage(
      odcore::data::image::SharedImage *a_sharedImage)
{
  bool isExtracted = false;
  std::shared_ptr<odcore::wrapper::SharedMemory> sharedMem(
      odcore::wrapper::SharedMemoryFactory::attachToSharedMemory(
          a_sharedImage->getName()));
  if (sharedMem->isValid()) {
    const uint32_t nrChannels = a_sharedImage->getBytesPerPixel();
    const uint32_t imgWidth = a_sharedImage->getWidth();
    const uint32_t imgHeight = a_sharedImage->getHeight();
    IplImage* myIplImage =
        cvCreateImage(cvSize(imgWidth,imgHeight), IPL_DEPTH_8U, nrChannels);
    cv::Mat bufferImage = cv::Mat(myIplImage);
    {
      sharedMem->lock();
      memcpy(bufferImage.data, sharedMem->getSharedMemory()
        , imgWidth*imgHeight*nrChannels);
      sharedMem->unlock();
    }
    m_img.release();
    m_img = bufferImage.clone();
    bufferImage.release();
    cvReleaseImage(&myIplImage);
    isExtracted = true;
  } else {
    std::cout << "[" << getName() << "] " << "Sharedmem is not valid." << std::endl;
  }
  return isExtracted;
}

void DetectCone::saveImg(double currentTime) {
  std::string img_name = std::to_string(currentTime);
  cv::imwrite("recording/"+img_name+".png", m_img);
}

void DetectCone::featureBased() {
  cv::Mat m_img_hsv, channel[3];
  cv::cvtColor(m_img, m_img_hsv, CV_RGB2HSV);
  cv::split(m_img_hsv, channel);
  //cv::adaptiveThreshold(adapThreshImg, channel[1], 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY_INV, m_adapThreshKernelSize, m_adapThreshConst);
  cv::imshow("img", channel[0]);
  cv::waitKey(0);
}

void DetectCone::blockMatching(cv::Mat& disp, cv::Mat imgL, cv::Mat imgR){
  cv::Mat grayL, grayR;

  cv::cvtColor(imgL, grayL, CV_BGR2GRAY);
  cv::cvtColor(imgR, grayR, CV_BGR2GRAY);

  cv::StereoBM sbm;
  sbm.state->SADWindowSize = 17;
  sbm.state->numberOfDisparities = 32;

  sbm(grayL, grayR, disp);
  cv::normalize(disp, disp, 0, 255, CV_MINMAX, CV_8U);
}

void DetectCone::rectify(cv::Mat img, cv::Mat& disp, cv::Mat& rectified){
  cv::Mat mtxLeft = (cv::Mat_<double>(3, 3) <<
    350.6847, 0, 332.4661,
    0, 350.0606, 163.7461,
    0, 0, 1);
  cv::Mat distLeft = (cv::Mat_<double>(5, 1) << -0.1674, 0.0158, 0.0057, 0, 0);
  cv::Mat mtxRight = (cv::Mat_<double>(3, 3) <<
    351.9498, 0, 329.4456,
    0, 351.0426, 179.0179,
    0, 0, 1);
  cv::Mat distRight = (cv::Mat_<double>(5, 1) << -0.1700, 0.0185, 0.0048, 0, 0);
  cv::Mat R = (cv::Mat_<double>(3, 3) <<
    0.9997, 0.0015, 0.0215,
    -0.0015, 1, -0.00008,
    -0.0215, 0.00004, 0.9997);
  //cv::transpose(R, R);
  cv::Mat T = (cv::Mat_<double>(3, 1) << -119.1807, 0.1532, 1.1225);

  cv::Size stdSize = cv::Size(640, 360);
  int width = img.cols;
  int height = img.rows;
  cv::Mat imgL(img, cv::Rect(0, 0, width/2, height));
  cv::Mat imgR(img, cv::Rect(width/2, 0, width/2, height));

  cv::resize(imgL, imgL, stdSize);
  cv::resize(imgR, imgR, stdSize);

  //std::cout << imgR.size() <<std::endl;

  cv::Mat R1, R2, P1, P2, Q;
  cv::Rect validRoI[2];
  cv::stereoRectify(mtxLeft, distLeft, mtxRight, distRight, stdSize, R, T, R1, R2, P1, P2, Q,
    cv::CALIB_ZERO_DISPARITY, 0.0, stdSize, &validRoI[0], &validRoI[1]);

  cv::Mat rmap[2][2];
  cv::initUndistortRectifyMap(mtxLeft, distLeft, R1, P1, stdSize, CV_16SC2, rmap[0][0], rmap[0][1]);
  cv::initUndistortRectifyMap(mtxRight, distRight, R2, P2, stdSize, CV_16SC2, rmap[1][0], rmap[1][1]);
  cv::remap(imgL, imgL, rmap[0][0], rmap[0][1], cv::INTER_LINEAR);
  cv::remap(imgR, imgR, rmap[1][0], rmap[1][1], cv::INTER_LINEAR);

  //cv::imwrite("2_left.png", imgL);
  //cv::imwrite("2_right.png", imgR);

  blockMatching(disp, imgL, imgR);
  rectified = imgL;
}

//run cnn starts
void DetectCone::convertImage(cv::Mat img, int w, int h, tiny_dnn::vec_t& data){
  cv::Mat resized;
  cv::resize(img, resized, cv::Size(w, h));
  data.resize(w * h * 3);
  for (int c = 0; c < 3; ++c) {
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
       data[c * w * h + y * w + x] =
         resized.at<cv::Vec3b>(y, x)[c] / 255.0;
      }
    }
  }
}

void DetectCone::slidingWindow(const std::string &dictionary, const std::string &img_path) {
  int PATCH_SIZE = 32;

  using conv    = tiny_dnn::convolutional_layer;
  using pool    = tiny_dnn::max_pooling_layer;
  using fc      = tiny_dnn::fully_connected_layer;
  using relu    = tiny_dnn::relu_layer;
  using softmax = tiny_dnn::softmax_layer;

  const size_t n_fmaps  = 32;  // number of feature maps for upper layer
  const size_t n_fmaps2 = 64;  // number of feature maps for lower layer
  const size_t n_fc     = 64;  // number of hidden units in fc layer
  tiny_dnn::core::backend_t backend_type = tiny_dnn::core::default_engine();
  tiny_dnn::network<tiny_dnn::sequential> nn;

  nn << conv(PATCH_SIZE, PATCH_SIZE, 5, 3, n_fmaps, tiny_dnn::padding::same, true, 1, 1,
             backend_type)                      // C1
     << pool(32, 32, n_fmaps, 2, backend_type)  // P2
     << relu()                                  // activation
     << conv(16, 16, 5, n_fmaps, n_fmaps, tiny_dnn::padding::same, true, 1, 1,
             backend_type)                      // C3
     << pool(16, 16, n_fmaps, 2, backend_type)  // P4
     << relu()                                  // activation
     << conv(8, 8, 5, n_fmaps, n_fmaps2, tiny_dnn::padding::same, true, 1, 1,
             backend_type)                                // C5
     << pool(8, 8, n_fmaps2, 2, backend_type)             // P6
     << relu()                                            // activation
     << fc(4 * 4 * n_fmaps2, n_fc, true, backend_type)    // FC7
     << relu()                                            // activation
     << fc(n_fc, 4, true, backend_type) << softmax(4);  // FC10

  // load nets
  std::ifstream ifs(dictionary.c_str());
  ifs >> nn;

  // convert imagefile to vec_t
  cv::Mat img = cv::imread(img_path);
  cv::Mat disp, rectified;
  rectify(img, disp, rectified);

  // manual roi
  // (453, 237,	0.96875,	"orange");
  // (585, 211,	0.625,	"orange");
  // (343, 185,	0.25,	"yellow");
  // (521, 198,	0.375,	"yellow");
  // (634,	202,	0.375,	"yellow");
  // (625,	191,	0.34375,	"blue");
  // (396,	183,	0.34375,	"blue");

  int x = 453;
  int y = 237;
  float_t ratio = 0.96875;
  std::string label = "orange";
  cv::Rect roi;
  int length = ratio * PATCH_SIZE;
  int radius = (length-1)/2;
  roi.x = x - radius;
  roi.y = y - radius;
  roi.width = length;
  roi.height = length;
  auto patch_img = rectified(roi);
  tiny_dnn::vec_t data;
  convertImage(patch_img, PATCH_SIZE, PATCH_SIZE, data);

  float_t resize_rate = 640.0/1280;
  float_t F = 350 / resize_rate;
  float_t d = 120;
  float_t factor = F * d;
  float_t depth = factor/disp.at<uchar>(y,x);

  // cv::circle(rectified, cv::Point (x,y), 3, cv::Scalar (0,0,0), CV_FILLED);
  // cv::circle(disp, cv::Point (x,y), 3, 0, CV_FILLED);
  // cv::namedWindow("disp", cv::WINDOW_NORMAL);
  // cv::imshow("disp", disp);
  // cv::waitKey(0);
  // cv::destroyAllWindows();

  // recognize
  auto prob = nn.predict(data);
  float_t threshold = 0.5;
  std::cout << prob[0] << " " << prob[1] << " " << prob[2] << " " << prob[3] << std::endl;
  int maxIndex = 1;
  float_t maxProb = prob[1];
  for(int i=2;i<4;i++){
    if(prob[i]>prob[maxIndex]){
      maxIndex = i;
      maxProb = prob[i];
    }
  }

  std::string labels[] = {"Yellow", "Blue", "Orange"};
  if (maxProb < threshold)
    std::cout << "No cone detected" << std::endl;
  else
    std::cout << "Find one " << labels[maxIndex-1] << " cone: " << x << ", " << y << ", " << depth << " mm" << std::endl;
}
//run cnn ends



}
}
}
}
