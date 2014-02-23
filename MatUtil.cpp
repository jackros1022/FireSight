#include <stdio.h>
#include "MatUtil.hpp"
#include "FireLog.h"
#include <iostream>

using namespace cv;
using namespace std;

static const char *cv_depth_names[] = {
	"CV_8U",
	"CV_8S",
	"CV_16U",
	"CV_16S",
	"CV_32S",
	"CV_32F",
	"CV_64F"
};

string matInfo(const Mat &m) {
	char buf[100];
	snprintf(buf, sizeof(buf), "%sC%d(%dx%d)", cv_depth_names[m.depth()], m.channels(), m.rows, m.cols);
	return string(buf);
}

Mat matRotateSize(Size sizeIn, Point2f center, double angle, double &minx, double &maxx, double &miny, double &maxy) {
	Mat transform = getRotationMatrix2D( center, angle, 1 );

	Matx<double,3,4> pts(
		0, sizeIn.width-1, sizeIn.width-1, 0,
		0, 0, sizeIn.height-1, sizeIn.height-1,
		1, 1, 1, 1);
	Mat mpts(pts);
	Mat newPts = transform * mpts;
	minx = newPts.at<double>(0,0);
	maxx = newPts.at<double>(0,0);
	miny = newPts.at<double>(1,0);
	maxy = newPts.at<double>(1,0);
	for (int c=1; c<4; c++) {
		double x = newPts.at<double>(0,c);
		minx = min(minx, x);
		maxx = max(maxx, x);
		double y = newPts.at<double>(1,c);
		miny = min(miny, y);
		maxy = max(maxy, y);
	}

	return transform;
}

void matWarpAffine(const Mat &image, Mat &result, Point2f center, double angle, double scale, 
	Point2f offset, Size size, int borderMode, Scalar borderValue, int flags)
{
	double minx;
	double maxx;
	double miny;
	double maxy;
	Mat transform = matRotateSize(Size(image.cols,image.rows), center, angle, minx, maxx, miny, maxy);

	transform.at<double>(0,2) += offset.x;
	transform.at<double>(1,2) += offset.y;

	Size resultSize(size);
	if (resultSize.width <= 0) {
		resultSize.width = maxx - minx + 1.5;
		transform.at<double>(0,2) += (resultSize.width-1)/2.0 - center.x;
	}
	if (resultSize.height <= 0) {
		resultSize.height = maxy - miny + 1.5;
    transform.at<double>(1,2) += (resultSize.height-1)/2.0 - center.y;
	}
	if (logLevel >= FIRELOG_TRACE) {
		char buf[200];
		LOGTRACE4("matWarpAffine() minx:%f, maxx:%f, %s-width:%d", 
			minx, maxx, (size.width <= 0 ? "auto" : "fixed"), resultSize.width);
		LOGTRACE4("matWarpAffine() miny:%f, maxy:%f, %s-height:%d", 
			miny, maxy, (size.height <= 0 ? "auto" : "fixed"), resultSize.height);
		snprintf(buf, sizeof(buf),"matWarpAffine() transform:[%f,%f,%f; %f,%f,%f]",
			transform.at<double>(0,0), transform.at<double>(0,1), transform.at<double>(0,2),
			transform.at<double>(1,0), transform.at<double>(1,1), transform.at<double>(1,2));
		LOGTRACE(buf);
	}

	Mat resultLocal;
	warpAffine( image, resultLocal, transform, resultSize, flags, borderMode, borderValue );
	result = resultLocal;
}


typedef enum {
	BEFORE_INFLECTION,
	AFTER_INFLECTION
} MinMaxState;

template<typename _Tp> _Tp _matMaxima(const cv::Mat &mat, std::vector<Point> &locations, _Tp rangeMin, _Tp rangeMax) {
	int rEnd = mat.rows-1;
	int cEnd = mat.cols-1;

	// CHECK EACH ROW MAXIMA FOR LOCAL 2D MAXIMA
	for (int r=0; r <= rEnd; r++) {
	  MinMaxState state = BEFORE_INFLECTION;
		_Tp curVal = mat.at<_Tp>(r,0);
		for (int c=1; c <= cEnd; c++) {
			_Tp val = mat.at<_Tp>(r,c);

			if (val == curVal) {
				continue;
			} else if (curVal < val) {
				if (state == BEFORE_INFLECTION) {
					// n/a
				} else {
					state = BEFORE_INFLECTION;
				}
			} else { // curVal > val
				if (state == BEFORE_INFLECTION) {
					if (rangeMin <= curVal && curVal <= rangeMax) { // ROW MAXIMA
						if (0<r && (mat.at<_Tp>(r-1,c-1) >= curVal || mat.at<_Tp>(r-1,c) >= curVal)) {
							// cout << "reject:r-1 " << r << "," << c-1 << endl;
						  // - x x
							// - - -
							// - - -
						} else if (r < rEnd && (mat.at<_Tp>(r+1,c-1) > curVal || mat.at<_Tp>(r+1,c) > curVal)) {
							// cout << "reject:r+1 " << r << "," << c-1 << endl;
							// - - -
							// - - -
						  // - x x
						} else if (1 < c && (0<r && mat.at<_Tp>(r-1,c-2) >= curVal || mat.at<_Tp>(r,c-2) > curVal || r < rEnd && mat.at<_Tp>(r+1,c-2) > curVal)) {
							// cout << "reject:c-2 " << r << "," << c-1 << endl;
							// x - -
							// x - -
						  // x - -
						} else {
							locations.push_back(Point(c-1,r));
						}
					}
					state = AFTER_INFLECTION;
				} else {
					// n/a
				}
			}

			curVal = val;
		}

		// PROCESS END OF ROW 
		if (state == BEFORE_INFLECTION) {
			if (rangeMin <= curVal && curVal <= rangeMax) { // ROW MAXIMA
				if (0<r && (mat.at<_Tp>(r-1,cEnd-1)>=curVal || mat.at<_Tp>(r-1,cEnd)>=curVal)) {
				  // cout << "rejectEnd:r-1 " << r << "," << cEnd-1 << endl;
					// - x x
					// - - -
					// - - -
				} else if (r<rEnd && (mat.at<_Tp>(r+1,cEnd-1)>curVal || mat.at<_Tp>(r+1,cEnd)>curVal)) {
				  // cout << "rejectEnd:r+1 " << r << "," << cEnd-1 << endl;
					// - - -
					// - - -
					// - x x
				} else if (1 < r && mat.at<_Tp>(r-1,cEnd-2) >= curVal || mat.at<_Tp>(r,cEnd-2) > curVal || r < rEnd && mat.at<_Tp>(r+1,cEnd-2) > curVal) {
				  // cout << "rejectEnd:cEnd-2 " << r << "," << cEnd-1 << endl;
					// x - -
					// x - -
					// x - -
				} else {
					locations.push_back(Point(cEnd,r));
				}
			}
		}
	}
}

/**
 * Return vector of Points indicating the locations of local maxima.
 * Multi-point maxima are represented with an upper-right-most point.
 */
void matMaxima(const cv::Mat &mat, std::vector<Point> &locations, float rangeMin, float rangeMax) {
	assert(mat.isContinuous());
	assert(mat.channels()==1);
	assert(mat.rows > 0 && mat.cols > 1);
	assert(mat.type() == CV_8U || mat.type() == CV_32F);

	LOGTRACE3("matMaxima(%s,,%f,%f)", matInfo(mat).c_str(), rangeMin, rangeMax);
	if (mat.type() == CV_8U) {
		_matMaxima<uchar>(mat, locations, (uchar)std::max(0.0f, rangeMin), (uchar)std::min(255.0f, rangeMax));
	} else if (mat.type() == CV_32F) {
		_matMaxima<float>(mat, locations, rangeMin, rangeMax);
	}
}


void matMinima(const cv::Mat &mat, std::vector<Point> &locations, float rangeMin, float rangeMax) {
}
