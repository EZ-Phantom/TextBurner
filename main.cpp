#include <opencv2/opencv.hpp>
#include "TextBurner.h"


int main () {
    cv::Mat mat = cv::imread("./lUYCkq-JIn8.jpg");
    try {
        netline::module::TextBurner burner("./cousine-regular.ttf");
        burner.setImage(&mat);

        burner.appendTextRow("a111 a11111111111 a1 a1 a1111111");
        burner.appendTextRow("b222222 b222 b2 bsdlhfjsdhf2 b222");
        burner.appendTextRow("open false promise");

        burner.burnAllTextZones();
    }
    catch (std::exception & ex) {
        std::cerr << ex.what();
    }

    cv::imshow("test", mat);
    cv::waitKey(0);

    return 0;
}

