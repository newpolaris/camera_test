//
// Created by peter.d.kim on 2020-10-12.
//

#ifndef CAMERA_CAM_UTILS_H
#define CAMERA_CAM_UTILS_H

#include <camera/NdkCameraManager.h>
#include <string>

namespace cam {

    std::string getBackFacingCamId(ACameraManager *cameraManager);

    void printCamProps(ACameraManager *cameraManager, const char *id);
}

#endif //CAMERA_CAM_UTILS_H
