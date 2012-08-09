#include <vector>
#include <iostream>

using namespace std;
using namespace colib;

namespace banglaocr {
	void createTesseractFormatImage(colib::intarray &I, colib::bytearray &RI, colib::intarray &SI);
	void makeProperBinaryMatLabFormatImage(colib::bytearray &orig_image, colib::bytearray &matlab_page_image);
	void getUnits(colib::bytearray &I, colib::intarray &UnitsInfo, int start, int end);
	void getMatraaLoc(colib::bytearray &TWord, int &ed_index_up_zone, int &st_index_ml_zone);
	void getMatraaLocPH(colib::bytearray &TWord, int &ed_index_up_zone, int &st_index_ml_zone);
	void getWords(colib::bytearray &I, colib::intarray &WordsInfo);
	void getWordsPHBased(colib::bytearray &I, colib::intarray &WordsInfo);
	void getLines(colib::bytearray &I, colib::intarray &LinesInfo);
	void getLinesPHBased(colib::bytearray &I, colib::intarray &Lines);
	int getBoxes(colib::bytearray &I, colib::intarray &Boxes);
	void rlhist(colib::bytearray &I, int* RLHist);
	void cropingImage(colib::bytearray &Simage, colib::bytearray &corped_image, int top,int bottom,int left,int right);
	void sortByRows(colib::intarray &arrayToSort, int** sorted_units_info, int arrSize, int numOfElementPerRow, int index);
	void sortByRows(int** arrayToSort, int** sorted_units_info, int arrSize, int numOfElementPerRow, int index);
	int getMaxVal(int a[], int limit);
	int getMaxIndx(int a[], int limit);
	int getMaxIndxInRange(int a[],int start,int end);
	int getMinVal(int a[], int limit);
	int getMinIndx(int a[], int limit);
	int getMinIndxInRange(int a[],int start,int end);
	int getSum(char flag,int num,colib::bytearray &I);
	int getSumLR(colib::bytearray &I, int num, int left, int right);
	float getMean(int a[], int N);
	int getMedian(int a[], int N);
	float getStdDev(int a[], int N);
	float getStdDevInRange(int a[],int start,int end);
	void getCrossing(colib::bytearray &I, int* CR);
	int getVerticalBar(colib::bytearray &I);
	int getConnectedComponentInformation(colib::bytearray &srcImg, colib::intarray &LabeledWord);
	int** allocate2DArray(int r, int c);
	void delete2DArray(int** array, int r);
	void copyDesiredColumn(int** srcArray, int* destArray, int totalRow, int columnToCopy);
	void copyDesiredRow(int** srcArray, int* destArray, int totalColumns, int rowToCopy);
	void segmentUnits(colib::intarray &result_segmentation,colib::bytearray &orig_image);
	void segmentUnitsOfBlockImage(colib::intarray &result_segmentation, colib::bytearray &orig_image);
	vector <vector<int> > eliminateSplittingError(vector<vector<int> > PUnits);
	vector <vector<int> > eliminateMBJError(vector<vector<int> > PUnits, colib::bytearray &Word);
	vector <int> eliminateTouchingError(colib::bytearray &I);
	vector <vector<int> > getUnitsFromTouchingPoints(vector<int> TPoints, colib::bytearray &SI);
	bool checkValidityUsingBottom(colib::bytearray &I);
	void convert_to_color_segmented_image(colib::intarray &result_segmentation,colib::bytearray &orig_image);
	void segmentUnits(colib::intarray &result_segmentation, colib::bytearray &orig_image);
	void prepareColorImageForLineImageTraining(colib::intarray &color_segmentation, colib::bytearray &orig_image);
	void prepareColorImageFromWordBoundary(colib::intarray &color_segmentation, colib::bytearray &orig_image);
	void crblpCharseg(colib::intarray &result_segmentation,colib::bytearray &orig_image);
}

