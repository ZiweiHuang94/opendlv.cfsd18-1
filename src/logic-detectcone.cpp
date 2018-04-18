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

#include "logic-detectcone.hpp"

void DetectCone::blockMatching(cv::Mat &disp, cv::Mat imgL, cv::Mat imgR){
  cv::Mat grayL, grayR;

  cv::cvtColor(imgL, grayL, CV_BGR2GRAY);
  cv::cvtColor(imgR, grayR, CV_BGR2GRAY);

  cv::Ptr<cv::StereoBM> sbm = cv::StereoBM::create(); 
  sbm->setBlockSize(17);
  sbm->setNumDisparities(32);

  sbm->compute(grayL, grayR, disp);
  cv::normalize(disp, disp, 0, 255, CV_MINMAX, CV_8U);
}

void DetectCone::reconstruction(cv::Mat img, cv::Mat &Q, cv::Mat &disp, cv::Mat &rectified, cv::Mat &XYZ){
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

  cv::Mat R1, R2, P1, P2;
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

  // cv::namedWindow("disp", cv::WINDOW_NORMAL);
  // cv::imshow("disp", disp);
  // cv::waitKey(0);

  rectified = imgL;

  cv::reprojectImageTo3D(disp, XYZ, Q);
  XYZ *= 0.002;
}

void DetectCone::convertImage(cv::Mat img, int w, int h, tiny_dnn::vec_t &data){
  cv::Mat resized;
  cv::resize(img, resized, cv::Size(w, h));
  data.resize(w * h * 3);
  for (int c = 0; c < 3; ++c) {
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
       data[c * w * h + y * w + x] =
         float_t(resized.at<cv::Vec3b>(y, x)[c] / 255.0);
      }
    }
  }
}

void DetectCone::xyz2xy(cv::Mat Q, cv::Point3d xyz, cv::Point2d &xy){
  double X = xyz.x / 2;
  double Y = xyz.y / 2;
  double Z = xyz.z / 2;
  double Cx = -Q.at<double>(0,3);
  double Cy = -Q.at<double>(1,3);
  double f = Q.at<double>(2,3);
  double a = Q.at<double>(3,2);
  double b = Q.at<double>(3,3);
  double d = (f - Z * b ) / ( Z * a);
  xy.x = X * ( d * a + b ) + Cx;
  xy.y = (Y+0.015) * ( d * a + b ) + Cy;
}

double DetectCone::depth2resizeRate(double x, double y){
  return 2*(1.6078-0.4785*std::sqrt(std::sqrt(x*x+y*y)));
}

void DetectCone::slidingWindow(const std::string &dictionary, tiny_dnn::network<tiny_dnn::sequential> &m_slidingWindow) {
  using conv    = tiny_dnn::convolutional_layer;
  using pool    = tiny_dnn::max_pooling_layer;
  using fc      = tiny_dnn::fully_connected_layer;
  using tanh    = tiny_dnn::tanh_layer;
  using relu    = tiny_dnn::relu_layer;
  using softmax = tiny_dnn::softmax_layer;

  tiny_dnn::core::backend_t backend_type = tiny_dnn::core::default_engine();

  m_slidingWindow << conv(25, 25, 4, 3, 16, tiny_dnn::padding::valid, true, 1, 1, backend_type) << tanh() 
     // << dropout(22*22*16, 0.25)                    
     << pool(22, 22, 16, 2, backend_type)                               
     << conv(11, 11, 4, 16, 32, tiny_dnn::padding::valid, true, 1, 1, backend_type) << tanh() 
     // << dropout(8*8*32, 0.25)                    
     << pool(8, 8, 32, 2, backend_type) 
     << fc(4 * 4 * 32, 128, true, backend_type) << relu()  
     << fc(128, 5, true, backend_type) << softmax(5);

  // load nets
  std::ifstream ifs(dictionary.c_str());
  ifs >> m_slidingWindow;
}

void DetectCone::backwardDetection(cv::Mat img, tiny_dnn::network<tiny_dnn::sequential> m_slidingWindow, std::vector<cv::Point3d> pts, std::vector<int>& outputs){
  //Given RoI in 3D world, project back to the camera frame and then detect
  double threshold = 0.7;
  cv::Mat Q, disp, rectified, XYZ;
  reconstruction(img, Q, disp, rectified, XYZ);
  std::vector<tiny_dnn::tensor_t> inputs;
  std::vector<int> verifiedIndex;
  std::vector<cv::Vec3i> porperty;
  outputs.clear();

  for(size_t i = 0; i < pts.size(); i++){
    cv::Point2d point2D;
    xyz2xy(Q, pts[i], point2D);

    int x = int(point2D.x);
    int y = int(point2D.y);

    // std::cout << "Camera region center: " << x << ", " << y << std::endl;
    double ratio = depth2resizeRate(pts[i].x, pts[i].z);
    if (ratio > 0) {
      int length = int(ratio * 25);
      int radius = (length-1)/2;
      // std::cout << "radius: " << radius << std::endl;

      cv::Rect roi;
      roi.x = std::max(x - radius, 0);
      roi.y = std::max(y - radius, 0);
      roi.width = std::min(x + radius, rectified.cols) - roi.x;
      roi.height = std::min(y + radius, rectified.rows) - roi.y;

      //cv::circle(img, cv::Point (x,y), radius, cv::Scalar (0,0,0));
      // // cv::circle(disp, cv::Point (x,y), 3, 0, CV_FILLED);
      //cv::namedWindow("roi", cv::WINDOW_NORMAL);
      //cv::imshow("roi", img);
      //cv::waitKey(0);
      //cv::destroyAllWindows();
      if (0 > roi.x || 0 > roi.width || roi.x + roi.width > rectified.cols || 0 > roi.y || 0 > roi.height || roi.y + roi.height > rectified.rows){
        std::cout << "Wrong roi!" << std::endl;
        outputs.push_back(-1);
      }
      else{
        auto patchImg = rectified(roi);
        tiny_dnn::vec_t data;
        convertImage(patchImg, 25, 25, data);
        inputs.push_back({data});
        outputs.push_back(0);
        verifiedIndex.push_back(i);
        porperty.push_back(cv::Vec3i(x,y,radius));
      }
    }
  }
  
  if(inputs.size()>0){
    auto prob = m_slidingWindow.predict(inputs);
    for(size_t i = 0; i < inputs.size(); i++){
      size_t maxIndex = 0;
      float_t maxProb = prob[i][0][0];
      for(size_t j = 1; j < 5; j++){
        if(prob[i][0][j] > maxProb){
          maxIndex = j;
          maxProb = prob[i][0][j];
        }
      }
      outputs[verifiedIndex[i]] = maxIndex;
      int x = int(porperty[i][0]);
      int y = int(porperty[i][1]);
      int radius = porperty[i][2];

      std::string labels[] = {"blue", "yellow", "orange", "big orange"};
      if (maxIndex == 0 || maxProb < threshold){
        std::cout << "No cone detected" << std::endl;
        cv::circle(rectified, cv::Point (x,y), radius, {0,0,0});
      } 
      else{
        std::cout << "Find one " << labels[maxIndex-1] << " cone"<< std::endl;
        if (labels[maxIndex-1] == "blue")
          cv::circle(rectified, cv::Point (x,y), radius, {255,0,0});
        else if (labels[maxIndex-1] == "yellow")
          cv::circle(rectified, cv::Point (x,y), radius, {0,255,255});
        else if (labels[maxIndex-1] == "orange")
          cv::circle(rectified, cv::Point (x,y), radius, {0,165,255});
        else if (labels[maxIndex-1] == "big orange")
          cv::circle(rectified, cv::Point (x,y), radius*2, {0,0,255});
      }
    }
  }

  // cv::namedWindow("disp", cv::WINDOW_NORMAL);
  // // cv::setWindowProperty("result", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);
  // cv::imshow("disp", rectified);
  // cv::waitKey(0);
  // cv::destroyAllWindows();

  // for(size_t i = 0; i < pts.size(); i++)
  //   std::cout << i << ": " << outputs[i] << std::endl;
}

void DetectCone::efficientSlidingWindow(const std::string &dictionary, tiny_dnn::network<tiny_dnn::sequential> &m_efficientSlidingWindow, int width, int height) {
  using conv    = tiny_dnn::convolutional_layer;
  using tanh    = tiny_dnn::tanh_layer;
  tiny_dnn::core::backend_t backend_type = tiny_dnn::core::backend_t::internal;

  m_efficientSlidingWindow << conv(width, height, 3, 3, 8, tiny_dnn::padding::valid, true, 1, 1, backend_type) << tanh()
     << conv(width-2, height-2, 3, 8, 8, tiny_dnn::padding::valid, true, 1, 1, backend_type) << tanh()
     // << dropout((width-4)*(height-4)*8, 0.25)
     << conv(width-4, height-4, 3, 8, 8, tiny_dnn::padding::valid, true, 1, 1, backend_type) << tanh()
     << conv(width-6, height-6, 3, 8, 8, tiny_dnn::padding::valid, true, 1, 1, backend_type) << tanh()
     // << dropout((width-8)*(height-8)*8, 0.25)
     << conv(width-8, height-8, 3, 8, 8, tiny_dnn::padding::valid, true, 1, 1, backend_type) << tanh()
     << conv(width-10, height-10, 3, 8, 16, tiny_dnn::padding::valid, true, 1, 1, backend_type) << tanh()
     // << dropout((width-12)*(height-12)*16, 0.25)
     << conv(width-12, height-12, 3, 16, 16, tiny_dnn::padding::valid, true, 1, 1, backend_type) << tanh()
     << conv(width-14, height-14, 3, 16, 16, tiny_dnn::padding::valid, true, 1, 1, backend_type) << tanh()
     // << dropout((width-16)*(height-16)*16, 0.25)
     << conv(width-16, height-16, 3, 16, 32, tiny_dnn::padding::valid, true, 1, 1, backend_type) << tanh()
     << conv(width-18, height-18, 3, 32, 32, tiny_dnn::padding::valid, true, 1, 1, backend_type) << tanh()
     // << dropout((width-20)*(height-20)*32, 0.25)
     << conv(width-20, height-20, 3, 32, 64, tiny_dnn::padding::valid, true, 1, 1, backend_type) << tanh()
     << conv(width-22, height-22, 3, 64, 5, tiny_dnn::padding::valid, true, 1, 1, backend_type);

  // load nets
  std::ifstream ifs(dictionary.c_str());
  ifs >> m_efficientSlidingWindow;
}

void DetectCone::softmax(cv::Vec<double,5> x, cv::Vec<double,5> &y) {
  double min, max, denominator = 0;
  cv::minMaxLoc(x, &min, &max);
  for (int j = 0; j < 5; j++) {
    y[j] = std::exp(x[j] - max);
    denominator += y[j];
  }
  for (int j = 0; j < 5; j++) {
    y[j] /= denominator;
  }
}

// float DetectCone::medianVector(std::vector<float> input){    
//   std::nth_element(input.begin(), input.begin() + input.size() / 2, input.end());
//   return input[input.size() / 2];
// }
// int DetectCone::medianVector(std::vector<std::pair<float, int>> input){
//   int n = input.size()/2;
//   std::nth_element(input.begin(), input.begin()+n, input.end());

//   // int median = input[n].first;
//   return input[n].second;
// }

std::vector <cv::Point> DetectCone::imRegionalMax(cv::Mat input, int nLocMax, double threshold, int minDistBtwLocMax)
{
    cv::Mat scratch = input.clone();
    // std::cout<<scratch<<std::endl;
    // cv::GaussianBlur(scratch, scratch, cv::Size(3,3), 0, 0);
    std::vector <cv::Point> locations(0);
    locations.reserve(nLocMax); // Reserve place for fast access
    for (int i = 0; i < nLocMax; i++) {
        cv::Point location;
        double maxVal;
        cv::minMaxLoc(scratch, NULL, &maxVal, NULL, &location);
        if (maxVal > threshold) {
            int row = location.y;
            int col = location.x;
            locations.push_back(cv::Point(col, row));
            int r0 = (row-minDistBtwLocMax > -1 ? row-minDistBtwLocMax : 0);
            int r1 = (row+minDistBtwLocMax < scratch.rows ? row+minDistBtwLocMax : scratch.rows-1);
            int c0 = (col-minDistBtwLocMax > -1 ? col-minDistBtwLocMax : 0);
            int c1 = (col+minDistBtwLocMax < scratch.cols ? col+minDistBtwLocMax : scratch.cols-1);
            for (int r = r0; r <= r1; r++) {
                for (int c = c0; c <= c1; c++) {
                    if (sqrt((r-row)*(r-row)+(c-col)*(c-col)) <= minDistBtwLocMax) {
                      scratch.at<double>(r,c) = 0.0;
                    }
                }
            }
        } else {
            break;
        }
    }
    return locations;
}

void DetectCone::forwardDetection(cv::Mat imgSource, tiny_dnn::network<tiny_dnn::sequential> m_efficientSlidingWindow) {
  double threshold = 0.9;
  int patchSize = 25;
  int patchRadius = int((patchSize-1)/2);
  int col = 320;
  int row = 180;
  int heightUp = 80;
  int heightDown = 140;
  int inputWidth = col;
  int inputHeight = heightDown-heightUp;

  int outputWidth  = inputWidth - (patchSize - 1);
  int outputHeight  = inputHeight - (patchSize - 1);

  cv::Rect roi;
  roi.x = 0;
  roi.y = heightUp;
  roi.width = inputWidth;
  roi.height = inputHeight;

  cv::Mat Q, disp, rectified, XYZ, resize_img;
  reconstruction(imgSource, Q, disp, rectified, XYZ);
  cv::resize(rectified, resize_img, cv::Size (col, row));

  auto patchImg = resize_img(roi);

  // convert imagefile to vec_t
  tiny_dnn::vec_t data;
  convertImage(patchImg, inputWidth, inputHeight, data);

  // recognize
  auto prob = m_efficientSlidingWindow.predict(data);

  cv::Mat probMap = cv::Mat::zeros(outputHeight, outputWidth, CV_64FC(5));
  for (int c = 0; c < 5; ++c)
    for (int y = 0; y < outputHeight; ++y)
      for (int x = 0; x < outputWidth; ++x)
         probMap.at<cv::Vec<double,5>>(y, x)[c] = prob[c * outputWidth * outputHeight + y * outputWidth + x];

  cv::Vec<double,5> probSoftmax(5);
  cv::Mat probMapSoftmax = cv::Mat::zeros(outputHeight, outputWidth, CV_64F);
  cv::Mat probMapIndex = cv::Mat::zeros(outputHeight, outputWidth, CV_32S);
  for (int y = 0; y < outputHeight; ++y){
    for (int x = 0; x < outputWidth; ++x){
      softmax(probMap.at<cv::Vec<double,5>>(y, x), probSoftmax);
      for (int c = 0; c < 4; ++c)
        if(probSoftmax[c+1] > threshold){
          probMapSoftmax.at<double>(y, x) = probSoftmax[c+1];
          probMapIndex.at<int>(y, x) = c+1;
        }
    }
  }

  std::vector <cv::Point> cone;
  cv::Point position, positionShift = cv::Point(patchRadius, patchRadius+heightUp);
  int label;
  cone = imRegionalMax(probMapSoftmax, 10, threshold, 20);
  // std::vector<std::pair<float, int>> coneRegion;
  // std::vector<cv::Point> coneRegionXY;

  if (cone.size()>0){
    for(size_t i = 0; i<cone.size(); i++){
      position = (cone[i] + positionShift)*2;
      if(position.x>320 && position.x<400 && position.y>250) continue;

      label = probMapIndex.at<int>(cone[i]);
      
      //median rectify
      // int ith = 0;
      // for(int x = positionResize.x-5; x <= positionResize.x+5; x++){
      //   for(int y = positionResize.y-5; y <= positionResize.y+5; y++){
      //     if (x >= 0 && x < 2*col && y >= 0 && y < 2*row){
      //       float depth = XYZ.at<cv::Point3d>(x, y)[2] * 2;
      //       if(depth > 0 && depth < 20000){
      //         coneRegion.push_back(std::pair<float, int>(depth, ith++));
      //         coneRegionXY.push_back(cv::Point(x, y));
      //       }
      //     }
      //   }  
      // }
      // if(coneRegion.size()>0){
      //   int medianIndex = medianVector(coneRegion);

      cv::Point3d point3D = XYZ.at<cv::Point3d>(position);
      // cv::Point3d point3D2 = XYZ.at<cv::Point3d>(coneRegionXY[medianIndex]) * 2;
      // std::cout << point3D2 << " " << point3D << std::endl;
      if (point3D.z > 0 && point3D.z < 10){
        if (label == 1){
          cv::circle(rectified, position, 3, {255, 0, 0}, -1);
          std::cout << "Find one blue cone, XYZ positon: "
          << point3D << "mm, xy position: " << position << "pixel, certainty: " 
          << probMapSoftmax.at<double>(cone[i]) << std::endl;
        }
        if (label == 2){
          cv::circle(rectified, position, 3, {0, 255, 255}, -1);
          std::cout << "Find one yellow cone, XYZ positon: "
          << point3D << "mm, xy position: " << position << "pixel, certainty: " 
          << probMapSoftmax.at<double>(cone[i]) << std::endl;
        }
        if (label == 3){
          cv::circle(rectified, position, 3, {0, 165, 255}, -1);
          std::cout << "Find one orange cone, XYZ positon: "
          << point3D << "mm, xy position: " << position << "pixel, certainty: " 
          << probMapSoftmax.at<double>(cone[i]) << std::endl;
        }
        if (label == 4){
          cv::circle(rectified, position, 6, {0, 0, 255}, -1);
          std::cout << "Find one big orange cone, XYZ positon: "
          << point3D << "mm, xy position: " << position << "pixel, certainty: " 
          << probMapSoftmax.at<double>(cone[i]) << std::endl;
        }
      }
    }
  }
}