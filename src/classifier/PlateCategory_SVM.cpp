#include "PlateCategory_SVM.h"
#include "CharInfo.h"


using namespace Doit::CV::PlateRecogn;

bool PlateCategory_SVM::IsReady = false;
cv::Size PlateCategory_SVM::HOGWinSize = cv::Size(96, 32);
cv::Size PlateCategory_SVM::HOGBlockSize = cv::Size(16, 16);
cv::Size PlateCategory_SVM::HOGBlockStride = cv::Size(8, 8);
cv::Size PlateCategory_SVM::HOGCellSize = cv::Size(8, 8);
int PlateCategory_SVM::HOGNBits = 9;
Ptr<SVM> PlateCategory_SVM::svm = null;
Random PlateCategory_SVM::random = Random();

void PlateCategory_SVM::SavePlateSample(PlateInfo &plateInfo, string fileName) {
    cv::imwrite(fileName, plateInfo.OriginalMat);
}

void PlateCategory_SVM::SavePlateSample(Mat &matPlate,
                                        PlateCategory_t &plateCategory,
                                        string libPath) {
    DateTime now = DateTime::Now();
    ostringstream buffer;
    buffer << now.Year << "-" << std::setfill('0') << std::setw(2) << now.Month
           << std::setfill('0') << std::setw(2) << "-" << std::setfill('0')
           << std::setw(2) << now.Day << "-" << std::setfill('0')
           << std::setw(2) << now.Hour << std::setfill('0') << std::setw(2)
           << "-" << std::setfill('0') << std::setw(2) << now.Minute << "-"
           << std::setfill('0') << std::setw(2) << now.Second << "-"
           << std::setfill('0') << std::setw(6) << random.Next(100000);
    string name = buffer.str();
    buffer.clear();

    buffer << libPath << "" DIRECTORY_DELIMITER "plates" DIRECTORY_DELIMITER ""
           << plateCategory << "" DIRECTORY_DELIMITER "" << name << ".jpg";
    string fileName = buffer.str();

    cv::imwrite(fileName, matPlate);
}
void PlateCategory_SVM::SavePlateSample(Mat &matPlate,
                                        PlateCategory_t plateCategory,
                                        string libPath,
                                        string shortFileNameNoExt) {
    ostringstream buffer;
    buffer << libPath << "" DIRECTORY_DELIMITER "plates" DIRECTORY_DELIMITER ""
           << plateCategory << "" DIRECTORY_DELIMITER "" << shortFileNameNoExt
           << ".jpg";
    string fileName = buffer.str();
    cv::imwrite(fileName, matPlate);
}

// use vector to replace array
vector<float> PlateCategory_SVM::ComputeHogDescriptors(Mat &image) {
    Mat matToHog;
    cv::resize(image, matToHog, HOGWinSize);
    HOGDescriptor hog = HOGDescriptor(HOGWinSize, HOGBlockSize, HOGBlockStride,
                                      HOGCellSize, HOGNBits);
    vector<float> ret;
    hog.compute(matToHog, ret, cv::Size(1, 1), cv::Size(0, 0));
    return ret;
}
bool PlateCategory_SVM::Train(Mat &samples, Mat &responses) {
    svm = SVM::create();
    svm->setType(SVM::Types::C_SVC);
    svm->setKernel(SVM::KernelTypes::LINEAR);
    svm->setTermCriteria(
        TermCriteria(TermCriteria::Type::MAX_ITER, 10000, 1e-10));
    IsReady = true;
    return svm->train(samples, SampleTypes::ROW_SAMPLE, responses);
}
void PlateCategory_SVM::Save(const string &fileName) {
    if (IsReady == false || svm == nullptr)
        return;
    svm->save(fileName);
}
void PlateCategory_SVM::Load(const string &fileName) {
    try {
        svm = SVM::load(fileName);
        IsReady = true;
    } catch (exception &) {
        throw logic_error("字符识别库加载异常，请检查存放路径");
    }
}
bool PlateCategory_SVM::IsCorrectTrainngDirectory(const string &path) {
    bool isCorrect = true;
    vector<const char *> plateCategoryNames(begin(PlateCategory_tToString),
                                            end(PlateCategory_tToString));

    for (size_t index = 0; index < plateCategoryNames.size(); index++) {
        string plateCategoryName = plateCategoryNames[index];
        ostringstream buffer;
        buffer << path << "" DIRECTORY_DELIMITER "" << plateCategoryName;
        string platePath = buffer.str();

        if (Directory::Exists(platePath) == false) {
            isCorrect = false;
            break;
        }
    }
    return isCorrect;
}
PlateCategory_t PlateCategory_SVM::Test(Mat &matTest) {
    try {
        if (IsReady == false || svm == null) {
            throw logic_error("训练数据为空，请重新训练⻋牌类型识别或加载数据");
        }

        PlateCategory_t result = PlateCategory_t::NonPlate;

        if (IsReady == false || svm == null)
            return result;
        vector<float> descriptor = ComputeHogDescriptors(matTest);
        Mat testDescriptor = Mat::zeros(1, descriptor.size(), CV_32FC1);
        for (size_t index = 0; index < descriptor.size(); index++) {
            testDescriptor.at<float>(0, index) = descriptor[index];
        }
        float predict = svm->predict(testDescriptor);
        result = (PlateCategory_t)((int)predict);
        return result;
    } catch (exception &ex) {
        std::cerr << ex.what() << std::endl;
        throw ex;
    }
}
PlateCategory_t PlateCategory_SVM::Test(const string &fileName) {
    Mat matTest = cv::imread(fileName, cv::ImreadModes::IMREAD_GRAYSCALE);
    return Test(matTest);
}

bool PlateCategory_SVM::PreparePlateTrainningDirectory(const string &path) {
    bool success = true;
    try {
        string platesDirectory = path + "" DIRECTORY_DELIMITER "plates";
        if (Directory::Exists(platesDirectory) == false)
            Directory::CreateDirectory(platesDirectory);
        vector<const char *> plateCategoryNames(begin(PlateCategory_tToString),
                                                end(PlateCategory_tToString));
        for (size_t index_PlateCategory = 0;
             index_PlateCategory < plateCategoryNames.size();
             index_PlateCategory++) {
            ostringstream buffer;
            buffer << platesDirectory << "" DIRECTORY_DELIMITER ""
                   << plateCategoryNames[index_PlateCategory];
            string plateCategoryDirectory = buffer.str();
            if (Directory::Exists(plateCategoryDirectory) == false)
                Directory::CreateDirectory(plateCategoryDirectory);
        }
    } catch (exception &) {
        success = false;
    }
    return success;
}