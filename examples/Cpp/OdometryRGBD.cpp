// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include <iostream>
#include <memory>

#include "Open3D/Open3D.h"

void PrintHelp(char* argv[]) {
    using namespace open3d;

    PrintOpen3DVersion();
    // clang-format off
    utility::PrintInfo("Usage:\n");
    utility::PrintInfo(">    OdometryRGBD [color_source] [source_target] [color_target] [depth_target] [options]\n");
    utility::PrintInfo("     Given RGBD image pair, estimate 6D odometry.\n");
    utility::PrintInfo("     [options]\n");
    utility::PrintInfo("     --camera_intrinsic [intrinsic_path]\n");
    utility::PrintInfo("     --rgbd_type [number] (0:Redwood, 1:TUM, 2:SUN, 3:NYU)\n");
    utility::PrintInfo("     --verbose : indicate this to display detailed information\n");
    utility::PrintInfo("     --hybrid : compute odometry using hybrid objective\n");
    // clang-format on
    utility::PrintInfo("\n");
}

int main(int argc, char* argv[]) {
    using namespace open3d;

    if (argc <= 4 || utility::ProgramOptionExists(argc, argv, "--help") ||
        utility::ProgramOptionExists(argc, argv, "-h")) {
        PrintHelp(argv);
        return 1;
    }

    std::string intrinsic_path;
    if (utility::ProgramOptionExists(argc, argv, "--camera_intrinsic")) {
        intrinsic_path = utility::GetProgramOptionAsString(argc, argv,
                                                           "--camera_intrinsic")
                                 .c_str();
        utility::PrintInfo("Camera intrinsic path %s\n",
                           intrinsic_path.c_str());
    } else {
        utility::PrintInfo("Camera intrinsic path is not given\n");
    }
    camera::PinholeCameraIntrinsic intrinsic;
    if (intrinsic_path.empty() ||
        !io::ReadIJsonConvertible(intrinsic_path, intrinsic)) {
        utility::PrintWarning(
                "Failed to read intrinsic parameters for depth image.\n");
        utility::PrintWarning("Use default value for Primesense camera.\n");
        intrinsic = camera::PinholeCameraIntrinsic(
                camera::PinholeCameraIntrinsicParameters::PrimeSenseDefault);
    }

    if (utility::ProgramOptionExists(argc, argv, "--verbose"))
        utility::SetVerbosityLevel(utility::VerbosityLevel::VerboseAlways);

    int rgbd_type =
            utility::GetProgramOptionAsInt(argc, argv, "--rgbd_type", 0);
    auto color_source = io::CreateImageFromFile(argv[1]);
    auto depth_source = io::CreateImageFromFile(argv[2]);
    auto color_target = io::CreateImageFromFile(argv[3]);
    auto depth_target = io::CreateImageFromFile(argv[4]);
    std::shared_ptr<geometry::RGBDImage> (*CreateRGBDImage)(
            const geometry::Image&, const geometry::Image&, bool);
    if (rgbd_type == 0)
        CreateRGBDImage = &geometry::RGBDImage::CreateFromRedwoodFormat;
    else if (rgbd_type == 1)
        CreateRGBDImage = &geometry::RGBDImage::CreateFromTUMFormat;
    else if (rgbd_type == 2)
        CreateRGBDImage = &geometry::RGBDImage::CreateFromSUNFormat;
    else if (rgbd_type == 3)
        CreateRGBDImage = &geometry::RGBDImage::CreateFromNYUFormat;
    else
        CreateRGBDImage = &geometry::RGBDImage::CreateFromRedwoodFormat;
    auto source = CreateRGBDImage(*color_source, *depth_source, true);
    auto target = CreateRGBDImage(*color_target, *depth_target, true);

    odometry::OdometryOption option;
    Eigen::Matrix4d odo_init = Eigen::Matrix4d::Identity();
    Eigen::Matrix4d trans_odo = Eigen::Matrix4d::Identity();
    Eigen::Matrix6d info_odo = Eigen::Matrix6d::Zero();
    bool is_success;
    if (utility::ProgramOptionExists(argc, argv, "--hybrid")) {
        odometry::RGBDOdometryJacobianFromHybridTerm jacobian_method;
        std::tie(is_success, trans_odo, info_odo) =
                odometry::ComputeRGBDOdometry(*source, *target, intrinsic,
                                              odo_init, jacobian_method,
                                              option);
    } else {
        odometry::RGBDOdometryJacobianFromColorTerm jacobian_method;
        std::tie(is_success, trans_odo, info_odo) =
                odometry::ComputeRGBDOdometry(*source, *target, intrinsic,
                                              odo_init, jacobian_method,
                                              option);
    }
    std::cout << "Estimated 4x4 motion matrix : " << std::endl;
    std::cout << trans_odo << std::endl;
    std::cout << "Estimated 6x6 information matrix : " << std::endl;
    std::cout << info_odo << std::endl;

    return int(!is_success);
}
