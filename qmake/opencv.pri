# OpenCV dependency selection:
# 1. OPENCV_ROOT/OPENCV_LIB_DIR from environment or local_paths.pri.
# 2. System OpenCV discovered through pkg-config.
!isEmpty(OPENCV_ROOT) {
    OPENCV_HEADER = $$OPENCV_ROOT/include/opencv4/opencv2/core.hpp
    !exists($$OPENCV_HEADER) {
        error("Invalid OPENCV_ROOT; header not found: $$OPENCV_HEADER")
    }

    isEmpty(OPENCV_LIB_DIR) {
        exists($$OPENCV_ROOT/lib/libopencv_core.so) {
            OPENCV_LIB_DIR = $$OPENCV_ROOT/lib
        } else:exists($$OPENCV_ROOT/lib64/libopencv_core.so) {
            OPENCV_LIB_DIR = $$OPENCV_ROOT/lib64
        } else:exists($$OPENCV_ROOT/lib/x86_64-linux-gnu/libopencv_core.so) {
            OPENCV_LIB_DIR = $$OPENCV_ROOT/lib/x86_64-linux-gnu
        } else:exists($$OPENCV_ROOT/lib/aarch64-linux-gnu/libopencv_core.so) {
            OPENCV_LIB_DIR = $$OPENCV_ROOT/lib/aarch64-linux-gnu
        } else {
            error("Invalid OPENCV_ROOT; libopencv_core.so not found: $$OPENCV_ROOT")
        }
    }

    INCLUDEPATH += $$OPENCV_ROOT/include/opencv4
    LIBS += -L$$OPENCV_LIB_DIR
    LIBS += -Wl,-rpath,$$OPENCV_LIB_DIR
    LIBS += -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs
    message("Using custom OpenCV root: $$OPENCV_ROOT")
    message("Using custom OpenCV library directory: $$OPENCV_LIB_DIR")
} else {
    CONFIG += link_pkgconfig
    packagesExist(opencv4) {
        PKGCONFIG += opencv4
        message("OPENCV_ROOT is not set; using system OpenCV from pkg-config")
    } else {
        error("OpenCV 4 was not found; install libopencv-dev or set OPENCV_ROOT")
    }
}
