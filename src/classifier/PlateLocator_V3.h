#ifndef PLATELOCATOR_V3_H
#define PLATELOCATOR_V3_H

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
using cv::cvtColor;
using cv::equalizeHist;
using cv::GaussianBlur;
using cv::Mat;
using cv::merge;
using cv::Rect;
using cv::Scalar;
using cv::split;
using cv::RotatedRect;

#include <vector>
using std::vector;
#include <string>
using std::string;

#include "CharInfo.h"
#include "PlateCategory_SVM.h"

namespace Doit {
namespace CV {
namespace PlateRecogn {
class PlateLocator_V3 {
  public:
    static bool VerifyPlateSize(const cv::Size &size, int minWidth = 60,
                                int maxWidth = 180, int minHeight = 18,
                                int maxHeight = 80, float minRatio = 0.15f,
                                float maxRatio = 0.70f) {
        int width = size.width;
        int height = size.height;
        if (width == 0 || height == 0)
            return false;
        float ratio = (float)height / width;
        bool result = (width > minWidth && width < maxWidth) &&
                      (height > minHeight && height < maxHeight) &&
                      (ratio > minRatio && ratio < maxRatio);
        return result;
    }

  public:
    static vector<PlateInfo> LocatePlatesForCameraAdjust(
        const Mat &matSource, Mat &matProcess, int blur_Size = 5,
        int sobel_Scale = 1, int sobel_Delta = 0, int sobel_X_Weight = 1,
        int sobel_Y_Weight = 0, int morph_Size_Width = 17,
        int morph_Size_Height = 3, int minWidth = 60, int maxWidth = 250,
        int minHeight = 18, int maxHeight = 100, float minRatio = 0.15f,
        float maxRatio = 0.70f) {
        vector<PlateInfo> plateInfos = LocatePlatesForAutoSample(
            matSource, matProcess, blur_Size, sobel_Scale, sobel_Delta,
            sobel_X_Weight, sobel_Y_Weight, morph_Size_Width, morph_Size_Height,
            minWidth, maxWidth, minHeight, maxHeight, minRatio, maxRatio);
        for (int index = plateInfos.size() - 1; index >= 0; index--) {
            if (plateInfos[index].PlateCategory == PlateCategory_t::NonPlate)
                plateInfos.erase(plateInfos.begin() + index);
        }
        return plateInfos;
    }

  public:
    static vector<PlateInfo> LocatePlatesForAutoSample(
        const Mat &matSource, Mat &matProcess, int blur_Size = 5,
        int sobel_Scale = 1, int sobel_Delta = 0, int sobel_X_Weight = 1,
        int sobel_Y_Weight = 0, int morph_Size_Width = 17,
        int morph_Size_Height = 3, int minWidth = 60, int maxWidth = 180,
        int minHeight = 18, int maxHeight = 80, float minRatio = 0.15f,
        float maxRatio = 0.70f) {
        vector<PlateInfo> plateInfos = vector<PlateInfo>();
        //优先使⽤颜⾊法定位可能是⻋牌的区域，如果没有发现⻋牌，再使⽤Sobel法
        Mat gray;
        Mat blur;
        if (matSource.empty() || matSource.rows == 0 || matSource.cols == 0) {
            matProcess = Mat(0, 0, CV_8UC1);
            return plateInfos;
        }
        // gray = matSource.CvtColor (ColorConversionCodes.BGR2GRAY);
        cvtColor(matSource, gray, cv::COLOR_BGR2GRAY);
        // blur = gray.GaussianBlur (new const cv::Size &(blur_Size, blur_Size),
        // 0, 0,
        //     BorderTypes.Default);
        // TODO size 构造不确定
        GaussianBlur(gray, blur, cv::Size(blur_Size, blur_Size), 0, 0,
                     cv::BorderTypes::BORDER_DEFAULT);
        // Mat hsv = matSource.CvtColor (ColorConversionCodes.BGR2HSV);
        Mat hsv;
        cv::cvtColor(matSource, hsv, cv::COLOR_BGR2HSV);
        // Mat[] hsvSplits = hsv.Split ();
        vector<Mat> hsvSplits;
        split(hsv, hsvSplits);
        // hsvSplits[2] = hsvSplits[2].EqualizeHist ();
        equalizeHist(hsvSplits[2], hsvSplits[2]);
        // Mat hsvEqualizeHist = new Mat ();
        Mat hsvEqualizeHist;
        // Cv2.Merge (hsvSplits, hsvEqualizeHist);
        merge(hsvSplits, hsvEqualizeHist);
        Scalar blueStart = cv::Scalar(100, 70, 70);
        Scalar blueEnd = cv::Scalar(140, 255, 255);
        // Mat blue = hsvEqualizeHist.InRange (blueStart, blueEnd);
        Mat blue;
        cv::inRange(hsvEqualizeHist, blueStart, blueEnd, blue);
        Scalar yellowStart = Scalar(15, 70, 70);
        Scalar yellowEnd = Scalar(40, 255, 255);
        // Mat yellow = hsvEqualizeHist.InRange (yellowStart, yellowEnd);
        Mat yellow;
        cv::inRange(hsvEqualizeHist, yellowStart, yellowEnd, yellow);
        Mat add = blue + yellow;
        // Mat threshold = add.Threshold (0, 255, ThresholdTypes.Otsu |
        // ThresholdTypes.Binary);
        Mat threshold;
        cv::threshold(add, threshold, 0, 255,
                      cv::ThresholdTypes::THRESH_OTSU |
                          cv::ThresholdTypes::THRESH_BINARY);
        // Mat element = Cv2.GetStructuringElement (MorphShapes.Rect,
        //     new const cv::Size &(morph_Size_Width,
        //         morph_Size_Height));
        Mat element = cv::getStructuringElement(
            cv::MorphShapes::MORPH_RECT,
            cv::Size(morph_Size_Width, morph_Size_Height));
        // Mat threshold_Close = threshold.MorphologyEx (MorphTypes.Close,
        // element);
        Mat threshold_Close;
        cv::morphologyEx(threshold, threshold_Close, cv::MorphTypes::MORPH_CLOSE, element);
        // Mat element_Erode = Cv2.GetStructuringElement (MorphShapes.Rect, new
        // const cv::Size &(3,
        //     3));
        Mat element_Erode = cv::getStructuringElement(
            cv::MorphShapes::MORPH_RECT, cv::Size(3, 3));
        // Mat threshold_Erode = threshold_Close.Erode (element_Erode);
        Mat threshold_Erode;
        cv::erode(threshold_Close, threshold_Erode, element_Erode);
        matProcess = threshold_Erode;
        // OpenCvSharp.Point[][] contours = null;
        vector<vector<cv::Point>> contours;
        // HierarchyIndex[] hierarchys = null;
        vector<cv::Vec4i> hierarchys;
        // Cv2.FindContours (threshold_Erode, out contours, out hierarchys,
        //     RetrievalModes.External, ContourApproximationModes.ApproxNone);
        cv::findContours(threshold_Erode, contours, hierarchys,
                         cv::RetrievalModes::RETR_EXTERNAL,
                         cv::ContourApproximationModes::CHAIN_APPROX_NONE);
        int isPlateCount = 0;
        for (int index = 0; index < contours.size(); index++) {
            // RotatedRect rotatedRect = cv::minAreaRect(contours[index]);
            RotatedRect rotatedRect = cv::minAreaRect(contours[index]);
            Rect rectROI = cv::boundingRect(contours[index]);
            if (VerifyPlateSize(rectROI.size(), minWidth, maxWidth, minHeight,
                                maxHeight, minRatio, maxRatio)) {
                Mat matROI = matSource(rectROI);
                PlateCategory_t plateCategory = PlateCategory_SVM::Test(matROI);
                if (plateCategory != PlateCategory_t::NonPlate)
                    isPlateCount++;
                PlateInfo plateInfo;
                plateInfo.RotatedRect = rotatedRect;
                plateInfo.OriginalRect = rectROI;
                plateInfo.OriginalMat = matROI;
                plateInfo.PlateCategory = plateCategory;
                plateInfo.PlateLocateMethod = PlateLocateMethod_t::Color;
                // plateInfos.Add(plateInfo);
                plateInfos.push_back(plateInfo);
            }
        }
        if (isPlateCount > 0)
            return plateInfos;

        // blur =
        //     matSource.GaussianBlur(new const cv::Size &(blur_Size, blur_Size),
        //                            0, 0, BorderTypes.Default);
        cv::GaussianBlur(matSource, blur, cv::Size(blur_Size, blur_Size), 0, 0, cv::BorderTypes::BORDER_DEFAULT);
        // gray = blur.CvtColor(ColorConversionCodes.BGR2GRAY);
        cv::cvtColor(blur, gray, cv::COLOR_BGR2GRAY);
        // 对图像进⾏Sobel 运算，得到的是图像的⼀阶⽔平⽅向导数。
        // Generate grad_x and grad_y
        // MatType ddepth = cv::CV_16S;
        auto ddepth = CV_16S;
        // Mat grad_x = gray.Sobel(ddepth, 1, 0, 3, sobel_Scale, sobel_Delta,
        //                         BorderTypes.Default);
        Mat grad_x;
        cv::Sobel(gray, grad_x, ddepth, 1, 0, 3, sobel_Scale, sobel_Delta, cv::BorderTypes::BORDER_DEFAULT);
        // Mat abs_grad_x = grad_x.ConvertScaleAbs();
        Mat abs_grad_x;
        cv::convertScaleAbs(grad_x, abs_grad_x);
        // Mat grad_y = gray.Sobel(ddepth, 0, 1, 3, sobel_Scale, sobel_Delta,
        //                         BorderTypes.Default);
        Mat grad_y;
        cv::Sobel(gray, grad_y, ddepth, 0, 1, 3, sobel_Scale, sobel_Delta, cv::BorderTypes::BORDER_DEFAULT);
        // Mat abs_grad_y = grad_y.ConvertScaleAbs();
        Mat abs_grad_y;
        cv::convertScaleAbs(grad_y, abs_grad_y);
        Mat grad = Mat();
        // Cv2.AddWeighted(abs_grad_x, sobel_X_Weight, abs_grad_y, sobel_Y_Weight,
        //                 0, grad);
        cv::addWeighted(abs_grad_x, sobel_X_Weight, abs_grad_y, sobel_Y_Weight, 0, grad);
        // 对图像进⾏⼆值化。将灰度图像（每个像素点有256
        // 个取值可能）转化为⼆值图像（每个像素点仅有1 和0 两个取值可能）。 
        // threshold =
        //     grad.Threshold(0, 255, ThresholdTypes.Otsu | ThresholdTypes.Binary);
        cv::threshold(grad, threshold, 0, 255, cv::ThresholdTypes::THRESH_OTSU | cv::ThresholdTypes::THRESH_BINARY);
        // 使⽤闭操作。对图像进⾏闭操作以后，可以看到⻋牌区域被连接成⼀个矩形装的区域。
        element = cv::getStructuringElement(cv::MorphShapes::MORPH_RECT,
            cv::Size(morph_Size_Width, morph_Size_Height));
        // threshold_Close = threshold.MorphologyEx(MorphTypes.Close, element);
        cv::morphologyEx(threshold, threshold_Close, cv::MorphTypes::MORPH_CLOSE, element);
        element_Erode = cv::getStructuringElement(cv::MorphShapes::MORPH_RECT, cv::Size(5, 5));
        // threshold_Erode = threshold_Close.Erode(element_Erode);
        cv::erode(threshold_Close, threshold_Erode, element_Erode);
        matProcess = threshold_Erode;

        // Find 轮廓 of possibles plates
        // 求轮廓。求出图中所有的轮廓。这个算法会把全图的轮廓都计算出来， 因此要进⾏ 筛选。 
        contours = {};
        hierarchys = {};
        cv::findContours(threshold_Erode, contours, hierarchys,
                         cv::RetrievalModes::RETR_EXTERNAL,
                         cv::ContourApproximationModes::CHAIN_APPROX_NONE);
        // 筛选。对轮廓求最⼩外接矩形，然后验证，不满⾜条件的淘汰。
        for (int index = 0; index < contours.size(); index++) {
            Rect rectROI = cv::boundingRect(contours[index]);
            if (VerifyPlateSize(rectROI.size(), minWidth, maxWidth, minHeight,
                                maxHeight, minRatio, maxRatio)) {
                RotatedRect rotatedRect = cv::minAreaRect(contours[index]);
                Mat matROI = matSource(rectROI);
                PlateCategory_t plateCategory = PlateCategory_SVM::Test(matROI);

                PlateInfo plateInfo;
                plateInfo.RotatedRect = rotatedRect;
                plateInfo.OriginalRect = rectROI;
                plateInfo.OriginalMat = matROI;
                plateInfo.PlateCategory = plateCategory;
                plateInfo.PlateLocateMethod = PlateLocateMethod_t::Sobel;
                // plateInfos.Add(plateInfo);
                plateInfos.push_back(plateInfo);
            }
        }
        return plateInfos;
    }

  public:
    static vector<PlateInfo>
    LocatePlates(const Mat &matSource, int blur_Size = 5, int sobel_Scale = 1,
                 int sobel_Delta = 0, int sobel_X_Weight = 1,
                 int sobel_Y_Weight = 0, int morph_Size_Width = 17,
                 int morph_Size_Height = 3, int minWidth = 60,
                 int maxWidth = 180, int minHeight = 18, int maxHeight = 80,
                 float minRatio = 0.15f, float maxRatio = 0.70f) {
        vector<PlateInfo> plateInfos = LocatePlatesByColor(
            matSource, blur_Size, morph_Size_Width, morph_Size_Height, minWidth,
            maxWidth, minHeight, maxHeight, minRatio, maxRatio);
        if (plateInfos.size() > 0)
            return plateInfos;
        plateInfos = LocatePlatesBySobel(
            matSource, blur_Size, sobel_Scale, sobel_Delta, sobel_X_Weight,
            sobel_Y_Weight, morph_Size_Width, morph_Size_Height, minWidth,
            maxWidth, minHeight, maxHeight, minRatio, maxRatio);
        return plateInfos;
    }
  private:
    static vector<PlateInfo>
    LocatePlatesByColor(const Mat &matSource, int blur_Size = 5,
                        int morph_Size_Width = 17, int morph_Size_Height = 3,
                        int minWidth = 60, int maxWidth = 180,
                        int minHeight = 18, int maxHeight = 80,
                        float minRatio = 0.15f, float maxRatio = 0.70f) {
        vector<PlateInfo> plateInfos = vector<PlateInfo>();
        if (matSource.empty())
            return plateInfos;
        // Mat hsv = matSource.CvtColor(ColorConversionCodes.BGR2HSV);
        Mat hsv;
        cv::cvtColor(matSource, hsv, cv::COLOR_BGR2HSV);

        // Mat[] hsvSplits = hsv.Split();
        vector<Mat> hsvSplits;
        cv::split(hsv, hsvSplits);
        // hsvSplits[2] = hsvSplits[2].EqualizeHist();
        cv::equalizeHist(hsvSplits[2], hsvSplits[2]);
        Mat hsvEqualizeHist;
        cv::merge(hsvSplits, hsvEqualizeHist);

        Scalar blueStart = Scalar(100, 70, 70);
        Scalar blueEnd = Scalar(140, 255, 255);
        // Mat blue = hsvEqualizeHist.InRange(blueStart, blueEnd);
        Mat blue;
        cv::inRange(hsvEqualizeHist, blueStart, blueEnd, blue);

        Scalar yellowStart = Scalar(15, 70, 70);
        Scalar yellowEnd = Scalar(40, 255, 255);
        // Mat yellow = hsvEqualizeHist.InRange(yellowStart, yellowEnd);
        Mat yellow;
        cv::inRange(hsvEqualizeHist, yellowStart, yellowEnd, yellow);
        Mat add = blue + yellow;
        // Mat threshold =
        //     add.Threshold(0, 255, ThresholdTypes.Otsu | ThresholdTypes.Binary);
        Mat threshold;
        cv::threshold(add, threshold, 0, 255, cv::ThresholdTypes::THRESH_OTSU | cv::ThresholdTypes::THRESH_BINARY);
        Mat element = cv::getStructuringElement(
            cv::MorphShapes::MORPH_RECT,
            cv::Size(morph_Size_Width, morph_Size_Height));
        // Mat threshold_Close = threshold.MorphologyEx(MorphTypes.Close, element);
        Mat threshold_Close;
        cv::morphologyEx(threshold, threshold_Close, cv::MorphTypes::MORPH_CLOSE, element);

        Mat element_Erode = cv::getStructuringElement(
            cv::MorphShapes::MORPH_RECT, cv::Size(3, 3));
        // Mat threshold_Erode = threshold_Close.Erode(element_Erode);
        Mat threshold_Erode;
        cv::erode(threshold_Close, threshold_Erode, element_Erode);

        vector<vector<cv::Point>> contours = {};
        vector<cv::Vec4i> hierarchys = {};
        cv::findContours(threshold_Erode, contours, hierarchys,
                         cv::RetrievalModes::RETR_EXTERNAL,
                         cv::ContourApproximationModes::CHAIN_APPROX_NONE);
        for (int index = 0; index < contours.size(); index++) {
            Rect rectROI = cv::boundingRect(contours[index]);
            if (VerifyPlateSize(rectROI.size(), minWidth, maxWidth, minHeight,
                                maxHeight, minRatio, maxRatio)) {
                Mat matROI = matSource(rectROI);
                PlateCategory_t plateCategory = PlateCategory_SVM::Test(matROI);
                if (plateCategory == PlateCategory_t::NonPlate)
                    continue;
                PlateInfo plateInfo = PlateInfo();
                plateInfo.OriginalRect = rectROI;
                plateInfo.OriginalMat = matROI;
                plateInfo.PlateCategory = plateCategory;
                plateInfo.PlateLocateMethod = PlateLocateMethod_t::Sobel;
                plateInfos.push_back(plateInfo);
            }
        }
        return plateInfos;
    }
  private:
    static vector<PlateInfo> LocatePlatesBySobel(
        Mat matSource, int blur_Size = 5, int sobel_Scale = 1,
        int sobel_Delta = 0, int sobel_X_Weight = 1, int sobel_Y_Weight = 0,
        int morph_Size_Width = 17, int morph_Size_Height = 3, int minWidth = 60,
        int maxWidth = 180, int minHeight = 18, int maxHeight = 80,
        float minRatio = 0.15f, float maxRatio = 0.70f) {
        vector<PlateInfo> plateInfos = vector<PlateInfo>();
        if (matSource.empty())
            return plateInfos;
        Mat blur;
        Mat gray;
        // blur =
        //     matSource.GaussianBlur(cv::Size(blur_Size, blur_Size),
        //                            0, 0, BorderTypes.Default);
        cv::GaussianBlur(matSource, blur, cv::Size(blur_Size, blur_Size), 0, 0, cv::BorderTypes::BORDER_DEFAULT);
        // gray = blur.CvtColor(ColorConversionCodes.BGR2GRAY);
        cv::cvtColor(blur, gray, cv::COLOR_BGR2GRAY);
        // 对图像进⾏Sobel 运算，得到的是图像的⼀阶⽔平⽅向导数。
        // Generate grad_x and grad_y
        auto ddepth = CV_16S;
        // Mat grad_x = gray.Sobel(ddepth, 1, 0, 3, sobel_Scale, sobel_Delta,
        //                         BorderTypes.Default);
        Mat grad_x;
        cv::Sobel(gray, grad_x, ddepth, 1, 0, 3, sobel_Scale, sobel_Delta,
                                cv::BorderTypes::BORDER_DEFAULT);
        // Mat abs_grad_x = grad_x.ConvertScaleAbs();
        Mat abs_grad_x;
        cv::convertScaleAbs(grad_x, abs_grad_x);
        // Mat grad_y = gray.Sobel(ddepth, 0, 1, 3, sobel_Scale, sobel_Delta,
        //                         BorderTypes.Default);
        Mat grad_y;
        cv::Sobel(gray, grad_y, ddepth, 0, 1, 3, sobel_Scale, sobel_Delta, cv::BorderTypes::BORDER_DEFAULT);
        // Mat abs_grad_y = grad_y.ConvertScaleAbs();
        Mat abs_grad_y;
        cv::convertScaleAbs(grad_y, abs_grad_y);
        Mat grad;
        cv::addWeighted(abs_grad_x, sobel_X_Weight, abs_grad_y, sobel_Y_Weight,
                        0, grad);
        
        // 对图像进⾏⼆值化。将灰度图像（每个像素点有256
        // 个取值可能）转化为⼆值图像（每个像素点仅有1 和0 两个取值可能）。 
        // Mat threshold =
        //     grad.Threshold(0, 255, ThresholdTypes.Otsu | ThresholdTypes.Binary);
        Mat threshold;
        cv::threshold(grad, threshold, 0, 255, cv::ThresholdTypes::THRESH_OTSU | cv::ThresholdTypes::THRESH_BINARY);
        
        // 使⽤闭操作。对图像进⾏闭操作以后，可以看到⻋牌区域被连接成⼀个矩形装的区域。
        Mat element = cv::getStructuringElement(
            cv::MorphShapes::MORPH_RECT,
            cv::Size(morph_Size_Width, morph_Size_Height));
        // Mat threshold_Close = threshold.MorphologyEx(MorphTypes.Close, element);
        Mat threshold_Close;
        cv::morphologyEx(threshold, threshold_Close, cv::MorphTypes::MORPH_CLOSE, element);

        Mat element_Erode = cv::getStructuringElement(
            cv::MorphShapes::MORPH_RECT, cv::Size(5, 5));
        // Mat threshold_Erode = threshold_Close.Erode(element_Erode);
        Mat threshold_Erode;
        cv::erode(threshold_Close, threshold_Close, element_Erode);
        // Find 轮廓 of possibles plates
        // 求轮廓。求出图中所有的轮廓。这个算法会把全图的轮廓都计算出来， 因此要进⾏ 筛选。 
        vector<vector<cv::Point>> contours = {};
        vector<cv::Vec4i> hierarchys = {};
        cv::findContours(threshold_Erode, contours, hierarchys,
                         cv::RetrievalModes::RETR_EXTERNAL,
                         cv::ContourApproximationModes::CHAIN_APPROX_NONE);
        // 筛选。对轮廓求最⼩外接矩形，然后验证，不满⾜条件的淘汰。
        for (int index = 0; index < contours.size(); index++) {
            Rect rectROI = cv::boundingRect(contours[index]);
            if (VerifyPlateSize(rectROI.size(), minWidth, maxWidth, minHeight,
                                maxHeight, minRatio, maxRatio)) {
                Mat matROI = matSource(rectROI);
                PlateCategory_t plateCategory = PlateCategory_SVM::Test(matROI);
                if (plateCategory == PlateCategory_t::NonPlate)
                    continue;
                PlateInfo plateInfo = PlateInfo();
                plateInfo.OriginalRect = rectROI;
                plateInfo.OriginalMat = matROI;
                plateInfo.PlateCategory = plateCategory;
                plateInfo.PlateLocateMethod = PlateLocateMethod_t::Sobel;
                plateInfos.push_back(plateInfo);
            }
        }
        return plateInfos;
    }
};

} // namespace PlateRecogn
} // namespace CV
} // namespace Doit


#endif // !PLATELOCATOR_V3_H
