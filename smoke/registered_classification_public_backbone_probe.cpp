#include "HalconCpp.h"
#include "HOperatorSet.h"

#include <iostream>
#include <string>

using namespace HalconCpp;

namespace {

void printTuple(const char *name, const HTuple &value)
{
    std::cout << name << ": " << value.ToString().Text() << std::endl;
}

void printParam(const HTuple &model, const char *name)
{
    try {
        HTuple value;
        GetDlModelParam(model, name, &value);
        printTuple(name, value);
    } catch (const HException &exception) {
        std::cout << name << ": <unavailable; HALCON " << exception.ErrorCode()
                  << ">" << std::endl;
    }
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "Usage: registered_classification_public_backbone_probe <model.onnx|model.hdl>"
                  << std::endl;
        return 2;
    }

    try {
        HTuple model;
        ReadDlModel(argv[1], &model);
        HTuple modelType;
        GetDlModelParam(model, "type", &modelType);
        printTuple("type_on_read", modelType);
        if (modelType.Length() == 1 && std::string(modelType[0].S().Text()) == "generic") {
            SetDlModelParam(model, "type", "classification");
            GetDlModelParam(model, "type", &modelType);
        }
        printTuple("type_for_probe", modelType);
        printParam(model, "image_width");
        printParam(model, "image_height");
        printParam(model, "image_num_channels");
        printParam(model, "image_range_min");
        printParam(model, "image_range_max");
        printParam(model, "summary");
        ClearDlModel(model);
        return 0;
    } catch (const HException &exception) {
        std::cerr << "HALCON error " << exception.ErrorCode() << ": "
                  << exception.ErrorMessage().Text() << std::endl;
        return 1;
    }
}
