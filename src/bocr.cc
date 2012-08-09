#include <colib/colib.h>
#include <iulib/iulib.h>
#include <ocropus/ocropus.h>

#include <fstream>
#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>

#include "bocr.h"

using namespace std;
using namespace colib;
using namespace iulib;
using namespace ocropus;

namespace banglaocr {
    
  void set_line_number(intarray &a, int lnum) {
        lnum <<= 12;
        for(int i = 0; i < a.length1d(); i++) {
            if (a.at1d(i) && a.at1d(i) != 0xFFFFFF)
                a.at1d(i) = (a.at1d(i) & 0xFFF) | lnum;
        }
    }
	class UnitInformation{
		// Attributes of this class are as follows:
		// 1. Top Index
		// 2. Bottom Index
		// 3. Left Index
		// 4. Right Index
		// 5. Image Height
		// 6. Image Width
		// 7. Matraa start Location
		// 8. Matraa end Location
		// 9. Line Number
		// 10. Word Number
		// 11. Unit Number
		// 12. Sub Unit Number
		// 13. Zone Information ( 1 - Upper Zone , 2 - Middle Zone & 3 - Lower Zone)
		// 14. Location of the shadow character. (value 0, if no shadow units)
		// 15. Color
	public:
		int top;
		int bottom;
		int left;
		int right;
		int height;
		int width;
		int matraaSrartLoc;
		int matraaEndLoc;
		int lineNumber;
		int wordNumber;
		int unitNumber;
		int subUnitNumber;
		int zoneNumber;
		int locOfShadowUnit;
		int colorNumber;
	};
	
	class point{
	public:
		int x;
		int y;
	};
	
	vector<point> find(int color,colib::intarray &I){
		point p;//=new point();
		vector<banglaocr::point> points;
		for(int i=0;i<I.dim(0);i++){
			for(int j=0;j<I.dim(1);j++){
				if(I(i,j)==color){
					p.x=i;
					p.y=j;
					points.push_back(p);
				}
			}
		}
		return points;
	}
	
	int getMaxLabel(colib::intarray &LabeledWord){
		int maxlable=0;
		for(int row=0; row<LabeledWord.dim(0); row++){
			for(int col=0;col<LabeledWord.dim(1);col++){
				if(LabeledWord(row,col)>maxlable)
					maxlable=LabeledWord(row,col);
			}
		}
		return maxlable;
	}
	
	/*
	Function for obtaining units of a particular line after segmenting the
	shadow or touching units. This function will take the step-1 segmented
	units and segment it further if there is any shadow or touching units
	
	input / parameter -> I : Image
	
	input / parameter -> LineWordsInfo : Information about the step-1
	segmented units.
	
	Output -> LineUnitsInfo : Information of the segmented units after
	aplying the shadow / touching character removal algorithm.
	*/
	
	vector<UnitInformation> getJoiningErrorFreeUnits(colib::bytearray &I, vector<UnitInformation> LineWordsInfo){
		vector<UnitInformation> LineUnitsInfo;
		
		//% LineUnitsCount : for LineUnitsInfo indexing
		int LineUnitsCount = -1;
		int totalUnits = LineWordsInfo.size();
		// cout<<"Total Number of Units :: "<<totalUnits<<endl;
		int** MZUnits = allocate2DArray(totalUnits, 2);
		
		//%%% Seperate Only the Middle Zone Units of the particular line
		int mzUnitCount = -1; //%% Middle Zone Unit Counter
		for(int tempCounter = 0; tempCounter<totalUnits; tempCounter++){
			//%% Condition : Contains middle zone -> Val - 2
			if(LineWordsInfo[tempCounter].zoneNumber == 2){
				mzUnitCount++;

				//%% Index of LineWordsInfo
				MZUnits[mzUnitCount][0] = tempCounter;
				
				//%% Width
				MZUnits[mzUnitCount][1] = LineWordsInfo[tempCounter].width;
			}
		}
		
		//cout<<"Total Units :: "<<totalUnits<<" MZUnits :: "<<mzUnitCount+1<<endl;
		
		//%%%%%==============-------- Find out the Maximum Character Width --------============%%%%%
		// Get the width information of the midddle zone units
		int* wdContainer = new int [mzUnitCount+1];
		
		// copy the desired (width) column only
		copyDesiredColumn(MZUnits, wdContainer, mzUnitCount+1, 1);
		
		/*cout<<"wdContainer ::"<<endl;
		for(int i = 0 ; i<totalUnits; i++){
			cout<<wdContainer[i]<<endl;
		}*/	
		
		// Get the 5 maximum width
		int tempWdCounter;
		
		if(mzUnitCount+1 <= 5)
			tempWdCounter = mzUnitCount+1;
		else
			tempWdCounter = 5;
		
		int* tempWdContainer = new int [tempWdCounter];
		
		int tempCount = -1;
		int maxWd, tempWdIndx;
		float wdVsHtRatio; 
		
		while(tempCount < tempWdCounter-1){
			maxWd = getMaxVal(wdContainer, tempWdCounter);
			tempWdIndx = getMaxIndx(wdContainer, tempWdCounter);
			wdVsHtRatio = (float) maxWd / LineWordsInfo[MZUnits[tempWdIndx][0]].height;
			
			if(wdVsHtRatio < 2){
				tempCount++;
				tempWdContainer[tempCount] = maxWd;
				wdContainer[tempWdIndx] = 0;
			}else{
				wdContainer[tempWdIndx] = maxWd / wdVsHtRatio;
			}
		}
		
		/*cout<<"tempWdContainer ::"<<endl;
		for(int i = 0 ; i<tempWdCounter; i++){
			cout<<tempWdContainer[i]<<endl;
		}*/
		
		int maxCharWd = getMedian(tempWdContainer, tempWdCounter);
		
		// free memory
		delete[] tempWdContainer;
		delete[] wdContainer;
		
		//%% Divide the subimage into three bins
		float wbin1Th = maxCharWd * 0.8;   //% threshold value for bin-1 classification
		float wbin2Th = maxCharWd * 0.64;  //% threshold value for bin-2 classification
		
		int wbin1Sz = -1; //% bin-1 size initialization
		int wbin2Sz = -1; //% bin-2 size initialization
		int wbin3Sz = -1; //% bin-3 size initialization
		
		int wdUnit;
		int** wBin1 = allocate2DArray(mzUnitCount+1, 2);
		int** wBin2 = allocate2DArray(mzUnitCount+1, 2);
		int** wBin3 = allocate2DArray(mzUnitCount+1, 2);
		
		for (int chrIndx = 0; chrIndx<=mzUnitCount; chrIndx++){
			//%% Width of the unit
			wdUnit = MZUnits[chrIndx][1];
			
			//% condition for bin classification
			if(wdUnit >= wbin1Th){
				wbin1Sz++;
				wBin1[wbin1Sz][0] = MZUnits[chrIndx][0];  //% store the character index
				wBin1[wbin1Sz][1] = wdUnit;   //% store the character width
			}else if(wdUnit < wbin1Th && wdUnit >= wbin2Th){
				wbin2Sz++;
				wBin2[wbin2Sz][0] = MZUnits[chrIndx][0];  //% store the character index
				wBin2[wbin2Sz][1] = wdUnit;   //% store the character width
			}else{
				wbin3Sz++;
				wBin3[wbin3Sz][0] = MZUnits[chrIndx][0];  //% store the character index
				wBin3[wbin3Sz][1] = wdUnit;   //% store the character width
			}
		}
		
		// free memory
		delete2DArray(MZUnits, totalUnits);
		
		// cout<<"Total Bin-1 Units :: "<<wbin1Sz+1<<endl;
		// cout<<"Total Bin-2 Units :: "<<wbin2Sz+1<<endl;
		// cout<<"Total Bin-3 Units :: "<<wbin3Sz+1<<endl;
		
		
		//%% Merge the units of Bin-1 and Bin-2 and store these into MBin
		int mBinCount = -1;
		int mBinSize = wbin1Sz + 1 + wbin2Sz + 1;
		int** tempMBin = allocate2DArray(mBinSize, 2);
		
		//%% Copy Bin-1 elements
		int wbin1Count = -1;
		if(wbin1Sz > -1){
			while(wbin1Count < wbin1Sz){
				// Increment : count
				mBinCount++;
				wbin1Count++;
				
				// Copy
				tempMBin[mBinCount][0] = wBin1[wbin1Count][0];
				tempMBin[mBinCount][1] = wBin1[wbin1Count][1];
			}
		}
		
		//cout<<"wbin2Count -- :: "<<endl;		
		// Copy Bin-2 elements
		int wbin2Count = -1;
		if(wbin2Sz > -1){
			while(wbin2Count < wbin2Sz){
				// Increment : count
				mBinCount++;
				wbin2Count++;
				
				// Copy
				tempMBin[mBinCount][0] = wBin2[wbin2Count][0];
				tempMBin[mBinCount][1] = wBin2[wbin2Count][1];
				//cout<<" :: "<<tempMBin[mBinCount][1]<<endl;
			}
		}
		
		
		int** MBin = allocate2DArray(mBinSize, 2);
		
		// Sort the units of MBin by the left index
		sortByRows(tempMBin, MBin, mBinSize, 2, 0);
		
		// Here I am finding the average width of the characters at Bin-2 as
		// they may have joining units with them. And Bin-1 Must contain
		// shadow or touching characters as per our observation of the output
		int avgWd;
		
		if(wbin2Count >= 0){
			int* tempArr = new int [wbin2Sz+1];
			
			copyDesiredColumn(wBin2, tempArr, wbin2Sz+1, 1);
			
			avgWd = floor(getMean(tempArr, wbin2Sz+1));   //% average width
			delete[] tempArr;
		}
		else{
			int* tempArr = new int [wbin1Sz+1];
			copyDesiredColumn(wBin1, tempArr, wbin1Sz+1, 1);
			avgWd = floor(getMean(tempArr, wbin1Sz+1));   //% average width
			delete[] tempArr;
		}
		
		// average width must be 3 or more [n several cases few units 
		// are passed to the CC labeler which have height/width less than 3. 
		// To prevent this we have to add certain conditions about the height 
		// and width of any unit.]
		if(avgWd < 3){
			avgWd = 3;
		}
		
		// free memory
		delete2DArray(wBin1, wbin1Sz);
		delete2DArray(wBin2, wbin2Sz);
		delete2DArray(wBin3, wbin3Sz);
		delete2DArray(tempMBin, mBinSize);
		
		// Variable need to count the number of units that contains shadow
		// characters and the algorithm is able to segment those
		int segmentedUnitsCount = -1;
		
		// Array need to store the information of the shadow characters.
		// Data Structure
		// 1. Index for linking shadow characters
		// 2. Left Index
		// 3. Right Index
		
		vector<vector<int> > segmentedUnitsInfo;
		
		tempCount = -1; //%% for counting the segmented units
		int thValHt = 5;
		int wdOfUnit;
		int imgIndx, unitTop, unitBottom, unitLeft, unitRight, unitHeight;
		// cout<<"Number of unit containing shadow characters : "<<mBinCount+1<<endl;
		// cout<<"Average Width : "<<avgWd<<endl;
		//%% search the image with shadow characters from bin-1
		//for(int mbinIndx = 5; mbinIndx<=5; mbinIndx++){
		for(int mbinIndx = 0; mbinIndx<=mBinCount; mbinIndx++){	
			//%% get the width of the unit
			wdOfUnit = MBin[mbinIndx][1];
			// cout<<"Width < "<<mbinIndx<<" > : "<<wdOfUnit<<endl;
			if(wdOfUnit >= avgWd && LineWordsInfo[MBin[mbinIndx][0]].height >= thValHt){
				//%% get the index of the image
				imgIndx = MBin[mbinIndx][0];
				
				//%% unitTop
				unitTop = LineWordsInfo[imgIndx].matraaEndLoc;
				
				//%% unitBottom
				unitBottom = LineWordsInfo[imgIndx].bottom;
				
				//%% unitLeft
				unitLeft = LineWordsInfo[imgIndx].left;
				
				//%% unitRight
				unitRight = LineWordsInfo[imgIndx].right;
				
				//%% unitHeight
				unitHeight = unitBottom - unitTop + 1;
				
				//%% crop the image after the matraa location
				intarray LabeledImage;
				bytearray SI;
				
				cropingImage(I, SI, unitTop, unitBottom, unitLeft, unitRight);
				
				//% get the connected components and extract the units
				int num = getConnectedComponentInformation(SI, LabeledImage);
				
				
				// Condition: split if there is more than one connected
				// component
				if(num > 1){
					// cout<<"if(num > 1)"<<endl;
					//cout<<" imgIndx :: "<<imgIndx<<endl;
					
					int** CC = allocate2DArray(num, 6);					
					for (int numOfCC = 0; numOfCC<num; numOfCC++){
						// get the information of the connected component
						vector<point> points = find(numOfCC+1, LabeledImage);
						
						int totalPoints = points.size();
						int* rInfo = new int [totalPoints];
						int* cInfo = new int [totalPoints];
						
						for(int numOfPoint=0; numOfPoint<totalPoints; numOfPoint++){
							rInfo[numOfPoint] = points[numOfPoint].x;
							cInfo[numOfPoint] = points[numOfPoint].y;
						}
						
						//% top
						CC[numOfCC][0] = getMinVal(rInfo, totalPoints);
						//% bottom
						CC[numOfCC][1] = getMaxVal(rInfo, totalPoints);
						//% left
						CC[numOfCC][2] = getMinVal(cInfo, totalPoints);
						//% right
						CC[numOfCC][3] = getMaxVal(cInfo, totalPoints);
						
						//cout<<CC[numOfCC][2]<<" - "<<CC[numOfCC][3]<<endl;
						
						//% height
						CC[numOfCC][4] = CC[numOfCC][1] - CC[numOfCC][0] + 1;
						//%width
						CC[numOfCC][5] = CC[numOfCC][3] - CC[numOfCC][2] + 1;
						
						delete[] rInfo;
						delete[] cInfo;
					}
					
					// sort the connected components information
					int** CCInfo = allocate2DArray(num, 6);
					sortByRows(CC, CCInfo, num, 6, 2);
					
					// free memory
					delete2DArray(CC, num);
					
					// set the threshold value
					int thVal = 3;
					
					// set the index of the segmented unit
					segmentedUnitsCount++;
					
					// Create link segmentedUnitsInfo <-> LineWordsInfo
					// save in LineWordsInfo
					LineWordsInfo[imgIndx].locOfShadowUnit = segmentedUnitsCount;
					
					//%% set the previous temporary unit count
					int prevTempCount = tempCount;
					
					int olapBTNCC, wdOfCC, topOfUnit, bottomOfUnit, leftOfUnit, rightOfUnit;
					
					for (int numOfCC = 0; numOfCC<num-1; numOfCC++){
						//cout<<"More than one CC"<<endl;
						// Calculate the overlap between the connected components
						olapBTNCC = CCInfo[numOfCC][3] - CCInfo[numOfCC+1][2] + 1;
						// Get the width of the connected component
						wdOfCC = CCInfo[numOfCC+1][3] - CCInfo[numOfCC][2] + 1;
						
						// Condition to satisfy a CC as a valid unit
						// olapBTNCC  < thVal : the overlap between two CC is less
						// than a threshold value
						// CCInfo(numOfCC+1, 1) < CCInfo(numOfCC+1, 2)/2 : The top
						// of the connected is below the half of the unit height
						// wdOfCC < thVal : Width of the connected component is
						// less than a threshold value
						// CCInfo(numOfCC, 2)> unitHeight/2 : The bottom of the CC
						// is less than half of the unit height
						
						if ((olapBTNCC  <= thVal) && (CCInfo[numOfCC+1][0] < CCInfo[numOfCC+1][1]/2) && (wdOfCC >= thVal) && (CCInfo[numOfCC][1]> unitHeight/2)){
							//cout<<"Overlapping..."<<endl;
							// calculate the top of the segmented unit
							topOfUnit = LineWordsInfo[imgIndx].matraaSrartLoc;
							
							//%% calculate the bottom of the segmented unit
							bottomOfUnit = unitTop + CCInfo[numOfCC][1];
							
							//%% calculate the left of the segmented unit
							leftOfUnit = unitLeft + CCInfo[numOfCC][2];
							
							//%% calculate the left of the segmented unit
							rightOfUnit = unitLeft + CCInfo[numOfCC][3];
							
							//%% increase the count
							tempCount++;
							
							// save info into :: segmentedUnitsInfo 
							vector<int> tempUnit;
							tempUnit.push_back(segmentedUnitsCount);
							tempUnit.push_back(topOfUnit);
							tempUnit.push_back(bottomOfUnit);
							tempUnit.push_back(leftOfUnit);
							tempUnit.push_back(rightOfUnit);
							
							segmentedUnitsInfo.push_back(tempUnit);
							
							tempUnit.clear();
						}else{
							//cout<<"Non Overlapping..."<<endl;
							// CCInfo[numOfCC+1][0] = min(CCInfo(numOfCC, 0) CCInfo(numOfCC+1, 0));
							if(CCInfo[numOfCC][0] < CCInfo[numOfCC+1][0])
								CCInfo[numOfCC+1][0] = CCInfo[numOfCC][0];
							
							// CCInfo[numOfCC+1][1] = max([CCInfo(numOfCC, 1) CCInfo(numOfCC+1, 1)]);
							if(CCInfo[numOfCC][1] > CCInfo[numOfCC+1][1])
								CCInfo[numOfCC+1][1] = CCInfo[numOfCC][1];
							
							// CCInfo[numOfCC+1][2] = min([CCInfo(numOfCC, 2) CCInfo(numOfCC+1, 2)]);
							if(CCInfo[numOfCC][2] < CCInfo[numOfCC+1][2])
								CCInfo[numOfCC+1][2] = CCInfo[numOfCC][2];
							
							// CCInfo[numOfCC+1][3] = max([CCInfo(numOfCC, 3) CCInfo(numOfCC+1, 3)]);
							if(CCInfo[numOfCC][3] > CCInfo[numOfCC+1][3])
								CCInfo[numOfCC+1][3] = CCInfo[numOfCC][3];
						}
					}
					
					//%% for the last component
					
					//%% calculate the top of the segmented unit
					topOfUnit = LineWordsInfo[imgIndx].matraaSrartLoc;
					
					//%% calculate the bottom of the segmented unit
					bottomOfUnit = unitTop + CCInfo[num-1][1];
					
					//%% calculate the left of the segmented unit
					leftOfUnit = unitLeft + CCInfo[num-1][2];
					
					//%% calculate the left of the segmented unit
					rightOfUnit = unitLeft + CCInfo[num-1][3];
					
					//%% Check whether there is a single unit or not
					if(prevTempCount == tempCount){
						int ht = SI.dim(0); 
						int wd = SI.dim(1);
						
						//%% Check whether the touching character segmentation method
						//%% will be appliable here or not
						if((float)ht/wd <= 0.8){
							//%% Top and Bottom of the units
							topOfUnit = LineWordsInfo[imgIndx].matraaSrartLoc;
							bottomOfUnit = LineWordsInfo[imgIndx].bottom;
							
							/*ofstream SaveFile("SI.txt");
							for(int row=0; row<SI.dim(0); row++){
								for(int col=0;col<SI.dim(1);col++){
									SaveFile <<(int)SI(row,col);
								}
								SaveFile <<endl;
							}
							SaveFile.close();*/
							
							//cout<<"Attempt to eliminate the touching errors [NumOfCC > 1] ..."<<endl;
							// Attempt to eliminate the touching errors withour the help of classifier
							//vector<int> TPoints= eliminateTouchingError(SI);//what to do
							
							vector<int> TPoints; // Temporary
							
							if(TPoints.size() > 0){
								vector<vector<int> > Units = getUnitsFromTouchingPoints(TPoints, SI);
								
								int numberOfUnit = Units.size();
								//cout<<"numberOfUnit ..."<<numberOfUnit<<endl;
								for(int printUnitCount = 0; printUnitCount<numberOfUnit; printUnitCount++){
									// save info into :: segmentedUnitsInfo 
									vector<int> tempUnit;
									tempUnit.push_back(segmentedUnitsCount);
									tempUnit.push_back(topOfUnit);
									tempUnit.push_back(bottomOfUnit);
									tempUnit.push_back(unitLeft + Units[printUnitCount][0] - 1);
									tempUnit.push_back(unitLeft + Units[printUnitCount][1] - 1);
									
									segmentedUnitsInfo.push_back(tempUnit);
									
									tempUnit.clear();
								}
							}
							else{ // if(TPoints(1) > 0){
								
								//cout<<"No touching element detected ..."<<endl;
								vector<int> tempUnit;
								tempUnit.push_back(segmentedUnitsCount);
								tempUnit.push_back(topOfUnit);
								tempUnit.push_back(bottomOfUnit);
								tempUnit.push_back(leftOfUnit);
								tempUnit.push_back(rightOfUnit);
								
								segmentedUnitsInfo.push_back(tempUnit);
								
								tempUnit.clear();
							}
							
							// free memory
							TPoints.clear();
							
						}else{ // if(ht/wd <= 0.8)
							// increase the count
							tempCount++;
							//cout<<"if(ht/wd <= 0.8)"<<endl;		
							// save info into :: segmentedUnitsInfo
							//cout<<"Image Index :: "<<imgIndx<<endl; 
							vector<int> tempUnit;
							tempUnit.push_back(segmentedUnitsCount);
							tempUnit.push_back(topOfUnit);
							tempUnit.push_back(bottomOfUnit);
							tempUnit.push_back(leftOfUnit);
							tempUnit.push_back(rightOfUnit);
							
							segmentedUnitsInfo.push_back(tempUnit);
							tempUnit.clear();
						} // if(ht/wd <= 0.8)
					}else{ // if(prevTempCount == tempCount)
						//%% increase the count
						tempCount++;
						
						// save info into :: segmentedUnitsInfo 
						//cout<<"Image Index :: "<<imgIndx<<endl;
						vector<int> tempUnit;
						tempUnit.push_back(segmentedUnitsCount);
						tempUnit.push_back(topOfUnit);
						tempUnit.push_back(bottomOfUnit);
						tempUnit.push_back(leftOfUnit);
						tempUnit.push_back(rightOfUnit);
						
						segmentedUnitsInfo.push_back(tempUnit);
						tempUnit.clear();
					} // end if(prevTempCount == tempCount)
					
					// free memory 
					delete2DArray(CCInfo, num);
					
				}else{    //% if(num > 1)
					//cout<<"if(num == 1)"<<endl;
					int ht = SI.dim(0); 
					int wd = SI.dim(1);
					
					if((float)ht/wd <= 0.8){
						//cout<<"Attempt to eliminate the touching errors [NumOfCC == 1] ..."<<endl;
						// Attempt to eliminate the touching errors withour the help of classifier
						vector<int> TPoints= eliminateTouchingError(SI);//what to do
						
						if(!TPoints.empty()){
							//cout<<"Getting touching locations ..."<<endl;
							vector<vector<int> > Units = getUnitsFromTouchingPoints(TPoints, SI);
							
							int numberOfUnit = Units.size();
							//cout<<"numberOfUnit ..."<<numberOfUnit<<endl;
							
							if(numberOfUnit > 1){
								// Create link segmentedUnitsInfo <-> LineWordsInfo
								
								// save in LineWordsInfo
								segmentedUnitsCount++;
								LineWordsInfo[imgIndx].locOfShadowUnit = segmentedUnitsCount;
								
								// Top and Bottom of the units
								int topOfUnit = LineWordsInfo[imgIndx].matraaSrartLoc - 1;
								int bottomOfUnit = LineWordsInfo[imgIndx].bottom;
								
								for(int printUnitCount = 0; printUnitCount<numberOfUnit; printUnitCount++){
									// save info into :: segmentedUnitsInfo 
									vector<int> tempUnit;
									tempUnit.push_back(segmentedUnitsCount);
									tempUnit.push_back(topOfUnit);
									tempUnit.push_back(bottomOfUnit);
									tempUnit.push_back(unitLeft + Units[printUnitCount][0] - 1);
									tempUnit.push_back(unitLeft + Units[printUnitCount][1] - 1);
									
									segmentedUnitsInfo.push_back(tempUnit);
									
									tempUnit.clear();
								}
							}
						}
						
						// free memory
						TPoints.clear();
						
					}  // end (ht/wd <= 0.8)				
				} // end if(num > 1)	
			} // end (wdOfUnit >= avgWd)
			
		} // end -> for  mbinIndx
		
		// free memory
		delete2DArray(MBin, mBinSize);
		
		//cout<<"Start Linkage between LineUnitsInfo && LineWordsInfo"<<endl;
		
		//%% Create the LineUnitsInfo using the link between LineWordsInfo and
		//%% segmentedUnitsInfo . Consider the value at LineWordsInfo(imgIndx, 13)
		//%% as the linking info. If the value is non-zero then use that value to
		//%% search the units at segmentedUnitsCount.
		
		
		// Condition: If the number of extracted shadow units is zero then we have
		// nothing to search.
		if (!segmentedUnitsInfo.empty()){
			
			// search the entire LineWordsInfo and build LineUnitsInfo using segmentedUnitsInfo
			
			// get the size of the LineWordsInfo array
			int numOfLineWordUnits = LineWordsInfo.size();
			//cout<<"numOfLineWordUnits :: "<<numOfLineWordUnits<<endl;
			// get the size of the segmentedUnitsInfo array
			
			int numOfSegUnits = segmentedUnitsInfo.size();
			//cout<<"numOfSegUnits :: "<<numOfSegUnits<<endl;
			// use a variable to track the searching index of the starting of
			// segmentedUnitsInfo array
			int tempIndex = 0;
			int indexSegUnit;
			
			for(int LineWordsCount = 0; LineWordsCount<numOfLineWordUnits; LineWordsCount++){
				//cout<<"LineWord :: [ "<<LineWordsCount+1<<" ] -- ";
				//%% use a variable to search the intended shadow units from
				//%% segmentedUnitsInfo
				indexSegUnit = LineWordsInfo[LineWordsCount].locOfShadowUnit;
				
				//%% condition : The searching index must be non-zero in order to
				//%% contain shadow units
				if(indexSegUnit != -1){
					
					// variable : for counting the number of units of a shadow units
					int countSegShUnit = 0;
					
					//%% Search the segmentedUnitsInfo array
					for(int searchIndex = tempIndex; searchIndex<numOfSegUnits; searchIndex++){
						
						//%% Condition : Check equality of the link
						if(segmentedUnitsInfo[searchIndex][0] == indexSegUnit){
							//%% Update : LineUnitsInfo
							
							//%% increment : countSegShUnit
							countSegShUnit++;
							
							//% increment : LineUnitsCount
							LineUnitsCount++;
							//cout<<"LineUnit :: [ "<<LineUnitsCount+1<<" ] -- ";
							UnitInformation uif;
							
							//% Line
							uif.lineNumber = LineWordsInfo[LineWordsCount].lineNumber;
							
							//% Word
							uif.wordNumber = LineWordsInfo[LineWordsCount].wordNumber;
							
							//% Unit
							uif.unitNumber = LineWordsInfo[LineWordsCount].unitNumber;
							
							//% Sub Unit
							uif.subUnitNumber = countSegShUnit;
							
							//% Top Index
							uif.top = segmentedUnitsInfo[searchIndex][1];
							
							//% Bottom Index
							uif.bottom = segmentedUnitsInfo[searchIndex][2];
							
							//% Left Index
							uif.left = segmentedUnitsInfo[searchIndex][3];
							
							//% Right Index
							uif.right = segmentedUnitsInfo[searchIndex][4];
							
							//% Height
							uif.height = uif.bottom - uif.top + 1;
							
							//% Width
							uif.width = uif.right - uif.left + 1;
							
							//% Zone Information
							uif.zoneNumber = 2;
							
							LineUnitsInfo.push_back(uif);
						}
					}
					
					//% increment / update : tempIndex
					tempIndex = tempIndex + countSegShUnit;
				}else{ // if(indexSegUnit != -1)
					//% increment LineUnitsCount
					LineUnitsCount++;
					//cout<<"LineUnit :: [ "<<LineUnitsCount+1<<" ] -- ";
					UnitInformation uif;
					
					//% Line
					uif.lineNumber = LineWordsInfo[LineWordsCount].lineNumber;
					
					//% Word
					uif.wordNumber = LineWordsInfo[LineWordsCount].wordNumber;
					
					//% Unit
					uif.unitNumber = LineWordsInfo[LineWordsCount].unitNumber;
					
					//% Sub Unit
					uif.subUnitNumber = 0;
					
					//% Top Index
					uif.top = LineWordsInfo[LineWordsCount].top;
					
					//% Bottom Index
					uif.bottom = LineWordsInfo[LineWordsCount].bottom;
					
					//% Left Index
					uif.left = LineWordsInfo[LineWordsCount].left;
					
					//% Right Index
					uif.right = LineWordsInfo[LineWordsCount].right;
					
					//% Height
					uif.height = LineWordsInfo[LineWordsCount].height;
					
					//% Width
					uif.width = LineWordsInfo[LineWordsCount].width;
					
					//% Zone Information
					uif.zoneNumber = LineWordsInfo[LineWordsCount].zoneNumber;
					
					LineUnitsInfo.push_back(uif);
					
					//cout<<"Just Inserted .."<<endl;
				}
			}
			
		}else{
			// get the size of the LineWordsInfo array
			int numOfLineWordUnits = LineWordsInfo.size();
			
			// Just Copy the necessary information
			for(int LineWordsCount = 0 ;LineWordsCount<numOfLineWordUnits; LineWordsCount++){
				// increment LineUnitsCount
				LineUnitsCount++;
				
				UnitInformation uif;
				
				//% Line
				uif.lineNumber = LineWordsInfo[LineWordsCount].lineNumber;
				
				//% Word
				uif.wordNumber = LineWordsInfo[LineWordsCount].wordNumber;
				
				//% Unit
				uif.unitNumber = LineWordsInfo[LineWordsCount].unitNumber;
				
				//% Sub Unit
				uif.subUnitNumber = 0;
				
				//% Top Index
				uif.top = LineWordsInfo[LineWordsCount].top;
				
				//% Bottom Index
				uif.bottom = LineWordsInfo[LineWordsCount].bottom;
				
				//% Left Index
				uif.left = LineWordsInfo[LineWordsCount].left;
				
				//% Right Index
				uif.right = LineWordsInfo[LineWordsCount].right;
				
				//% Height
				uif.height = LineWordsInfo[LineWordsCount].height;
				
				//% Width
				uif.width = LineWordsInfo[LineWordsCount].width;
				
				//% Zone Information
				uif.zoneNumber = LineWordsInfo[LineWordsCount].zoneNumber;
				
				LineUnitsInfo.push_back(uif);
			}
		}
		//cout<<"Size of LineUnitsInfo ::: "<<LineUnitsInfo.size()<<endl;	
		return LineUnitsInfo;
	}
	
	
	//%% function for eliminating the splitting errors
	
	//%% input / parameter -> LineUnitsInfo : Array that contains the entire Line
	//%% Units Information
	
	//%% Output -> SEFLineUnitsInfo : Splitting error free Line Units information
	
	vector<UnitInformation> getSplittingErrorFreeUnitsOfALine(vector<UnitInformation> LineUnitsInfo){
		vector<UnitInformation> SEFLineUnitsInfo;
		
		// Get the size of the LineUnitsInfo
		int numOfLineUnits = LineUnitsInfo.size();
		//cout<<"numOfLineUnits :: "<<numOfLineUnits<<endl;
		
		// Get the line number
		int lineNumber = LineUnitsInfo[numOfLineUnits-1].lineNumber;
		
		// Get the line number
		int totalWord = LineUnitsInfo[numOfLineUnits-1].wordNumber;
		//cout<<"totalWord :: "<<totalWord<<endl;
		
		//%% Initialize variables
		int unitCounter = -1;
		int wordNumber = -1;
		int searchStartIndex = 0;
		
		
		while(unitCounter < numOfLineUnits-1){
			//%% get the words information
			wordNumber++;
			int tempMZUCounter = -1;
			int tempUZUCounter = -1;
			
			vector<vector<int> > MZUInfo; // Middle Zone Unit Information 
			vector<vector<int> > UZUInfo; // Upper Zone Unit Information 
			
			for(int inDx = searchStartIndex; inDx<numOfLineUnits; inDx++){
				//%% Get the Word information
				if(LineUnitsInfo[inDx].wordNumber == wordNumber){
					//%% Increment
					unitCounter++;
					
					if(LineUnitsInfo[inDx].zoneNumber == 2){
						//%% Increment : tempMZUCounter
						tempMZUCounter++;
						
						vector<int> tempUnit;
						
						//% Top Index
						tempUnit.push_back(LineUnitsInfo[unitCounter].top);
						
						//% Bottom Index
						tempUnit.push_back(LineUnitsInfo[unitCounter].bottom);
						
						//% Left Index
						tempUnit.push_back(LineUnitsInfo[unitCounter].left);
						
						//% Right Index
						tempUnit.push_back(LineUnitsInfo[unitCounter].right);
						
						//% Height Index
						tempUnit.push_back(LineUnitsInfo[unitCounter].height);
						
						//% Width Index
						tempUnit.push_back(LineUnitsInfo[unitCounter].width);
						
						//% Zone Information
						tempUnit.push_back(2);
						
						MZUInfo.push_back(tempUnit);
						//cout<<inDx<<"  - "<<"Top - "<<LineUnitsInfo[unitCounter].top<<", bottom - "<<LineUnitsInfo[unitCounter].bottom<<", left - "<<LineUnitsInfo[unitCounter].left<<", right - "<<LineUnitsInfo[unitCounter].right<<", zone - "<<LineUnitsInfo[unitCounter].zoneNumber<<endl;
						tempUnit.clear();
					}else{
						//%% Increment : tempUZUCounter
						tempUZUCounter++;
						
						vector<int> tempUnit;
						
						//% Top Index
						tempUnit.push_back(LineUnitsInfo[unitCounter].top);
						
						//% Bottom Index
						tempUnit.push_back(LineUnitsInfo[unitCounter].bottom);
						
						//% Left Index
						tempUnit.push_back(LineUnitsInfo[unitCounter].left);
						
						//% Right Index
						tempUnit.push_back(LineUnitsInfo[unitCounter].right);
						
						//% Height Index
						tempUnit.push_back(LineUnitsInfo[unitCounter].height);
						
						//% Width Index
						tempUnit.push_back(LineUnitsInfo[unitCounter].width);
						
						//% Zone Information
						tempUnit.push_back(1);
						
						UZUInfo.push_back(tempUnit);
						//cout<<inDx<<"  - "<<"Top - "<<LineUnitsInfo[unitCounter].top<<", bottom - "<<LineUnitsInfo[unitCounter].bottom<<", left - "<<LineUnitsInfo[unitCounter].left<<", right - "<<LineUnitsInfo[unitCounter].right<<", zone - "<<LineUnitsInfo[unitCounter].zoneNumber<<endl;
						tempUnit.clear();
					}
				}else{
					searchStartIndex = searchStartIndex + tempMZUCounter+1 + tempUZUCounter+1;
					break;
				}
			}
			//cout<<"wordNumber :: "<<wordNumber<<",  unitCounter :: "<<unitCounter+1<<endl;
			
			//%% Eliminate the Splitting error (If any) from the word units
			vector<vector<int> > MZInfo = eliminateSplittingError(MZUInfo);
			
			int count = MZInfo.size() - 1;
			
			int startIndex;
			int endIndex;
			
			//cout<<"count :: "<<count<<",  tempMZUCounter :: "<<tempMZUCounter+1<<endl;
			//%% Add the Upper zone units with the Middle zone units
			if(count == tempMZUCounter){
				//cout<<"count == tempMZUCounter"<<endl;
				
				// Simple copy the word information from the LineUnitsInfo to
				// SEFLineUnitsInfo as there is no change
				
				if(wordNumber == totalWord){
					startIndex = searchStartIndex;
					endIndex   = numOfLineUnits-1;
				}else{
					startIndex = searchStartIndex - (MZUInfo.size() + UZUInfo.size());
					endIndex   = searchStartIndex - 1;
				}
				//cout<<"startIndex :: "<<startIndex<<",  endIndex :: "<<endIndex<<endl;
				
				int tempUnitCount = -1; //% for Indexing the units
				
				for(int wordCopyIndx = startIndex ; wordCopyIndx<=endIndex; wordCopyIndx++){
					
					UnitInformation uif;
					
					//%% Line Number
					uif.lineNumber = lineNumber;
					
					//%% Word Number
					uif.wordNumber = wordNumber;
					
					//%% Unit Number
					tempUnitCount++;
					
					uif.unitNumber = tempUnitCount;
					
					//%% Top Index
					uif.top = LineUnitsInfo[wordCopyIndx].top;
					
					//%% Bottom Index
					uif.bottom = LineUnitsInfo[wordCopyIndx].bottom;
					
					//%% Left Index
					uif.left = LineUnitsInfo[wordCopyIndx].left;
					
					//%% Right Index
					uif.right = LineUnitsInfo[wordCopyIndx].right;
					
					//%% Height
					uif.height = LineUnitsInfo[wordCopyIndx].height;
					
					//%% Width
					uif.width = LineUnitsInfo[wordCopyIndx].width;
					
					//%% Zone Information
					uif.zoneNumber = LineUnitsInfo[wordCopyIndx].zoneNumber;
					//cout<<wordCopyIndx<<"  - "<<"Top - "<<uif.top<<", bottom - "<<uif.bottom<<", left - "<<uif.left<<", right - "<<uif.right<<", zone - "<<uif.zoneNumber<<endl;
					SEFLineUnitsInfo.push_back(uif);
				}
				
			}else{ //% (count < tempMZUCounter)
				//cout<<"count < tempMZUCounter"<<endl;
				
				// Merge the middle zone and upper zone units
				int totalUnits = MZInfo.size() + UZUInfo.size();
				//cout<<"totalUnits :: "<<totalUnits<<endl;
				
				int infoPerRow = 7;
				int** tempUnitsInfo = allocate2DArray(totalUnits, infoPerRow);
				int** UnitsInfo = allocate2DArray(totalUnits, infoPerRow);
				int unitCounter = -1;
				
				// Add Middle Zone Information
				for(int tempr = 0; tempr<MZInfo.size() ;tempr++){
					unitCounter++;
					for(int tempc = 0; tempc<infoPerRow ;tempc++){
						tempUnitsInfo[unitCounter][tempc] = MZInfo[tempr][tempc];
					}
				}
				
				// Add Upper Zone Information
				for(int tempr = 0; tempr<UZUInfo.size() ;tempr++){
					unitCounter++;
					for(int tempc = 0; tempc<infoPerRow ;tempc++){
						tempUnitsInfo[unitCounter][tempc] = UZUInfo[tempr][tempc];
					}
				}
				
				// Sort Units Info based on the left index
				sortByRows(tempUnitsInfo, UnitsInfo, totalUnits, infoPerRow, 2);
				
				// free memory
				delete2DArray(tempUnitsInfo, totalUnits);
				
				// Copy the units info into : SEFLineUnitsInfo 
				startIndex = 0;
				endIndex = count + tempUZUCounter + 1; 
				//cout<<"endIndex :: "<<endIndex<<endl;
				
				for(int wordCopyIndx = startIndex;wordCopyIndx<=endIndex;wordCopyIndx++){
					UnitInformation uif;
					
					//%% Line Number
					uif.lineNumber = lineNumber;
					
					//%% Word Number
					uif.wordNumber = wordNumber;
					
					//%% Unit Number
					uif.unitNumber = wordCopyIndx;
					
					//%% Top Index
					uif.top = UnitsInfo[wordCopyIndx][0];
					
					//%% Bottom Index
					uif.bottom = UnitsInfo[wordCopyIndx][1];
					
					//%% Left Index
					uif.left = UnitsInfo[wordCopyIndx][2];
					
					//%% Right Index
					uif.right = UnitsInfo[wordCopyIndx][3];
					
					//%% Height
					uif.height = UnitsInfo[wordCopyIndx][4];
					
					//%% Width
					uif.width = UnitsInfo[wordCopyIndx][5];
					
					//%% Zone Information
					uif.zoneNumber = UnitsInfo[wordCopyIndx][6];
					//cout<<wordCopyIndx<<"  - "<<"Top - "<<uif.top<<", bottom - "<<uif.bottom<<", left - "<<uif.left<<", right - "<<uif.right<<", zone - "<<uif.zoneNumber<<endl;
					SEFLineUnitsInfo.push_back(uif);
				}
				
				// free memory
				delete2DArray(UnitsInfo, totalUnits);
			} //% (count ( < or == ) tempMZUCounter)
			
			
		} // end while
		
		return SEFLineUnitsInfo;
	}
	
	// function for eliminating the splitting errors
	// input / parameter -> PUnits : Array that contains the priliminary
	// Units Information
	// Output -> Units : Splitting error free Units information
	
	vector<vector<int> > eliminateSplittingError(vector<vector<int> > PUnits){
		
		vector<vector<int> > Units;
		
		// Data Structure : PUnits
		//// 1. Top Index
		//// 2. Bottom Index
		//// 3. Left Index
		//// 4. Right Index
		//// 5. Image Height
		//// 6. Image Width
		//// 7. Matraa Start
		//// 8. Matraa End
		//// 9. Zone Inforamtion
		
		// Get the size of the priliminary Units
		int pUnitSize = PUnits.size();
		
		// Calculate the statistical data : mean/average, median and standard deviation
		int* tempArray = new int [pUnitSize];
		// copy the image height column
		for(int tmp=0; tmp<pUnitSize; tmp++){
			tempArray[tmp] = PUnits[tmp][4];
		} 
		
		float avgPUnitsBottom = getMean(tempArray, pUnitSize);
		float medianPUnitsBottom = getMedian(tempArray, pUnitSize);
		float stdPUnitsBottom = getStdDev(tempArray, pUnitSize);
		int maxPUnitsBottom = getMaxVal(tempArray, pUnitSize);
		
		// Threshold unit bottom location
		float thUnitBtLoc1 = 0;
		float thUnitBtLoc2, thUnitBtLoc2_1, thUnitBtLoc2_2;
		
		if(maxPUnitsBottom - medianPUnitsBottom > stdPUnitsBottom){
			thUnitBtLoc2_1 = avgPUnitsBottom - stdPUnitsBottom;
		}else{
			thUnitBtLoc2_1 = maxPUnitsBottom - (avgPUnitsBottom/5 + stdPUnitsBottom);
		}
		
		thUnitBtLoc2_2 = maxPUnitsBottom - (avgPUnitsBottom/5 + stdPUnitsBottom);
		
		if(thUnitBtLoc2_1 < thUnitBtLoc2_2){
			thUnitBtLoc2 = ceil(thUnitBtLoc2_1);
		}else{
			thUnitBtLoc2 = ceil(thUnitBtLoc2_2);
		}
		
		// Threshold next unit width vs height ratio (To determine that the next one is a vertical bar or not)
		float thUnitWdHtRatio = 0.4;
		
		// Threshold next unit width vs height ratio (To determine that the next one is a vertical bar or not)
		float thUnitHtHtRatio = 0.75;
		
		int unitsCount = -1; // for counting the number of units
		
		// Get number of priliminary units
		int numOfPUnits = pUnitSize;
		int startOfUnit;
		float ratioOfWdHtNextUnit, zValue;
		
		if(numOfPUnits > 1){
			// Initialize the start of unit variable
			startOfUnit = PUnits[0][2];
			
			for(int inDxCount = 0; inDxCount<numOfPUnits-1; inDxCount++){
				// Get the ratio of width vs height of the next unit
				ratioOfWdHtNextUnit = (float)PUnits[inDxCount+1][5] / PUnits[inDxCount+1][4];
				zValue = (PUnits[inDxCount][4] - avgPUnitsBottom ) / ((float)stdPUnitsBottom + 0.001);
				if((zValue < thUnitBtLoc1) && (PUnits[inDxCount][4] <= thUnitBtLoc2) && (ratioOfWdHtNextUnit < thUnitWdHtRatio) && ((float)PUnits[inDxCount][4] / PUnits[inDxCount+1][4] < thUnitHtHtRatio)){
					startOfUnit = PUnits[inDxCount][2];
				}else{
					unitsCount++;
					
					vector<int> tempUnit;
					
					tempUnit.push_back(PUnits[inDxCount][0]);
					tempUnit.push_back(PUnits[inDxCount][1]);
					tempUnit.push_back(startOfUnit);
					tempUnit.push_back(PUnits[inDxCount][3]);
					tempUnit.push_back(PUnits[inDxCount][4]);
					tempUnit.push_back(tempUnit[3] - tempUnit[2] + 1);
					tempUnit.push_back(PUnits[inDxCount][6]);
					tempUnit.push_back(PUnits[inDxCount][7]);
					tempUnit.push_back(PUnits[inDxCount][8]);
					
					Units.push_back(tempUnit);
					tempUnit.clear();
					startOfUnit = PUnits[inDxCount+1][2];
				}
			}
			
			// For the last unit
			unitsCount++;
			
			vector<int> tempUnit;
			
			tempUnit.push_back(PUnits[numOfPUnits-1][0]);
			tempUnit.push_back(PUnits[numOfPUnits-1][1]);
			tempUnit.push_back(startOfUnit);
			tempUnit.push_back(PUnits[numOfPUnits-1][3]);
			tempUnit.push_back(PUnits[numOfPUnits-1][4]);
			tempUnit.push_back(tempUnit[3] - tempUnit[2] + 1);
			tempUnit.push_back(PUnits[numOfPUnits-1][6]);
			tempUnit.push_back(PUnits[numOfPUnits-1][7]);
			tempUnit.push_back(PUnits[numOfPUnits-1][8]);
			
			Units.push_back(tempUnit);
			tempUnit.clear();
			// return value
			return Units;
		}else{
			return PUnits;
		}
	}
	
	// function for eliminating the matraa based joining errors
	// input / parameter -> PUnits : Array that contains the priliminary Units Information
	// input / parameter -> Word : Image of the word
	
	// Output -> Units : Splitting error free Units information
	
	vector<vector<int> > eliminateMBJError(vector<vector<int> > PUnits, bytearray &Word){
		vector<vector<int> > Units;
		
		//// Data Structure : PUnits
		//// 1. Top Index
		//// 2. Bottom Index
		//// 3. Left Index
		//// 4. Right Index
		//// 5. Image Height
		//// 6. Image Width
		//// 7. Matraa Start
		//// 8. Matraa End
		//// 9. Zone Inforamtion
		
		// Get the size of the priliminary Units
		int numOfUnit = PUnits.size();
		float aspRatio;
		bytearray TI;
		int r, c, totalPixel;
		float thVal, thValLastMatraaRow;
		bool changeStatus;
		
		for(int num=0; num<numOfUnit; num++){
			aspRatio = (float)PUnits[num][5] / PUnits[num][4];
			
			if(aspRatio > 1.2){
				
				// Get the unit image
				cropingImage(Word, TI, PUnits[num][7], PUnits[num][1], PUnits[num][2], PUnits[num][3]);
				
				r = TI.dim(0);
				c = TI.dim(1);
				
				thVal = c * 0.75;
				thValLastMatraaRow = c * 0.65;
				changeStatus = false;
				
				// Get Horizontal Histogram and compare with respect to unit width
				for(int i=0; i<ceil((float)r/4); i++){
					totalPixel = c - getSum('r', i, TI);
					if(totalPixel >= thVal){
						// TI(i, :) = 1;
						for(int j=0; j<c; j++){
							TI(i, j) = 1;
						}
						
						changeStatus = true;
					}
					else if(totalPixel >= thValLastMatraaRow){ // Special condition for the last matraa row checking
						// TI(i, :) = 1;
						for(int j=0; j<c; j++){
							TI(i, j) = 1;
						}
						
						changeStatus = true;
						break;
					}
					else{
						break;
					}
				}
				
				if(changeStatus){
					// calculate vertical histogram
					int VH[c];
					for(int i=0; i<c; i++){
						VH[i] = r - getSum('c',i,TI); // VH: contains the vertical histogram result
					}
					
					// Extract the boundary points
					int count = -1;
					
					int CB[c]; // Character Boundary array
					int startSearchIndex = 0; // Searching index for the vertical histogram
					
					// for the first non-zero column : special case when the first column
					// histogram value is non-zero
					//cout<<"VH :: ";
					if(VH[0]!=0){
						count++;
						CB[count] = -1; // CB - Character Boundary
						//	cout<<CB[count]<<", ";
						startSearchIndex = 1;
					}
					
					// find all the zero valued location from the histogram
					for( int i = startSearchIndex;i<c;i++){
						if(VH[i]==0){
							count++;
							CB[count] = i; // CB - Character Boundary
							//		cout<<CB[count]<<", ";
						}
					}
					
					// for the last non-zero column : special case when the last column
					// histogram value is non-zero
					
					if(VH[c-1]!=0){
						count++;
						CB[count] = c; // CB - Character Boundary
						//	cout<<CB[count]<<", ";
					}
					//cout<<endl;
					
					// find out the boundary points
					thVal = 1;
					int left_, right_, width_;
					
					for(int i = 1; i<=count; i++){
						if(CB[i] - CB[i-1] > thVal){
							vector<int> tempUnit;
							
							// left and right
							left_ = CB[i-1] + 1;
							right_ = CB[i] - 1;
							width_ = right_ - left_ + 1;
							
							// top
							tempUnit.push_back(PUnits[num][0]);
							
							// bottom
							tempUnit.push_back(PUnits[num][1]);
							
							//cout<<"Top :: "<<tempUnit[0]<<", Bottom ::: "<<tempUnit[1]<<endl; 
							// left
							tempUnit.push_back(PUnits[num][2] + left_ - 1);
							
							// right
							tempUnit.push_back(PUnits[num][2] + right_ - 1);
							
							//cout<<"Left :: "<<tempUnit[2]<<", Right ::: "<<tempUnit[3]<<endl;
							
							// height
							tempUnit.push_back(PUnits[num][4]);
							
							// width
							tempUnit.push_back(width_);
							
							// matraa start loc
							tempUnit.push_back(PUnits[num][6]);
							
							// matraa end loc
							tempUnit.push_back(PUnits[num][7]);
							
							// Zone Information
							tempUnit.push_back(2);
							
							Units.push_back(tempUnit);
							
							tempUnit.clear();
						}
					}
				}else{ // if(changeStatus)
					// Simply copy
					// Units(countUnit, :) = PUnits(num, :);
					
					vector<int> tempUnit;
					
					// top
					tempUnit.push_back(PUnits[num][0]);
					
					// bottom
					tempUnit.push_back(PUnits[num][1]);
					
					// left
					tempUnit.push_back(PUnits[num][2]);
					
					// right
					tempUnit.push_back(PUnits[num][3]);
					
					// height
					tempUnit.push_back(PUnits[num][4]);
					
					// width
					tempUnit.push_back(PUnits[num][5]);
					
					// matraa start loc
					tempUnit.push_back(PUnits[num][6]);
					
					// matraa end loc
					tempUnit.push_back(PUnits[num][7]);
					
					// Zone Information
					tempUnit.push_back(PUnits[num][8]);
					
					Units.push_back(tempUnit);
					
					tempUnit.clear();
				}
			}else{ // if(aspRatio > 1.2)
				// Simply copy
				// Units(countUnit, :) = PUnits(num, :);
				
				vector<int> tempUnit;
				
				// top
				tempUnit.push_back(PUnits[num][0]);
				
				// bottom
				tempUnit.push_back(PUnits[num][1]);
				
				// left
				tempUnit.push_back(PUnits[num][2]);
				
				// right
				tempUnit.push_back(PUnits[num][3]);
				
				// height
				tempUnit.push_back(PUnits[num][4]);
				
				// width
				tempUnit.push_back(PUnits[num][5]);
				
				// matraa start loc
				tempUnit.push_back(PUnits[num][6]);
				
				// matraa end loc
				tempUnit.push_back(PUnits[num][7]);
				
				// Zone Information
				tempUnit.push_back(PUnits[num][8]);
				
				Units.push_back(tempUnit);
				
				tempUnit.clear();
			} // if(aspRatio > 1.2)
		}
		// return statement
		return Units;
	}

	
	// function for eliminating the joinning errors
	
	// input / parameter -> I : Image of the touching character
	// Output -> TPoints : Location of the touching points
	
	vector<int> eliminateTouchingError(colib::bytearray &I){
		bytearray TI;
		vector<int> TPoints;
		
		/*ofstream SaveFile("TI.txt");
		for(int row=0; row<I.dim(0); row++){
			for(int col=0;col<I.dim(1);col++){
				SaveFile <<(int)I(row,col);
			}
			SaveFile <<endl;
		}
		SaveFile.close();*/
			
		// Threshold width for searching. This is because we are considering the
		// minimum width of any object width as the threshold width
		int thWd = 2;
		int thHt = 2; //% Threshold height
		
		int ht = I.dim(0);
		int wd = I.dim(1);
		
		/////////////  Do Row-wise OR operation ///////////////
		// Initialize TI
		copy(TI,I);
		
		
		// Perform the OR Operation
		for(int hti=thHt; hti<ht-1; hti++){
			for(int wdi=thWd; wdi<wd-thWd; wdi++){
				TI(hti, wdi) = (I(hti, wdi) | I(hti+1, wdi));
			}
		}
		
		// make the last row : as zero
		for(int wdi=0; wdi<wd; wdi++){
			TI(ht-1, wdi) = 1;	
		}
		
		// search for the segmentation location
		for(int wdi = thWd+1; wdi<wd-thWd+1; wdi++){
			if((ht - getSum('c',wdi,TI)) == 0){
				TPoints.push_back(wdi);
			}
		}
		
		/*ofstream SaveFile("TI.txt");
		for(int row=0; row<I.dim(0); row++){
			for(int col=0;col<I.dim(1);col++){
				SaveFile <<(int)I(row,col);
			}
			SaveFile <<endl;
		}
		SaveFile.close();*/
		
		//cout<<"\tPerformed Row-wise OR Operation"<<endl;
		//cout<<"After Row-wise OR Operation :: "<<TPoints.size()<<endl;
		/////////////  Do columnwise OR operation ///////////////
		// Initialize TI
		if(TPoints.empty()){
			// Initialize TI
			copy(TI, I);
			
			// Perform the OR Operation
			for(int wdi=thWd+1; wdi<wd-thWd; wdi++){
				for(int hti = 0;hti<ht-1;hti++){
					TI(hti, wdi) = (I(hti, wdi) | I(hti, wdi+1));
				}
			}
			
			// search for the segmentation location
			for(int wdi = thWd+1;wdi<wd-thWd+1;wdi++)
			if((ht - getSum('c',wdi,TI)) == 0){
				TPoints.push_back(wdi);
			}
		}
		
		//cout<<"\t\tPerformed columnwise OR Operation"<<endl;
		//cout<<"After columnwise OR Operation :: "<<TPoints.size()<<endl;
		if(TPoints.empty()){
			//cout<<"\t\t\tStart 3rd attempt.."<<endl;
			
			//<STEP 1>
			// get the crossing information
			int* CR = new int[wd];
			bytearray tempTI;
			cropingImage(TI, tempTI, 1, ht-1, 0, wd-1);
			getCrossing(tempTI, CR);	
			//cout<<"\t\t\t-> got the crossing information"<<endl;
			
			//<STEP 2>
			// get the pixel information
			int* PI = new int [wd];
			
			// initialize the pixel information
			for(int tempCount = 0; tempCount<thWd; tempCount++){
				PI[tempCount] = -1;
			}
			
			for(int tempCount = thWd; tempCount<wd-thWd+1; tempCount++){
				PI[tempCount] = ht - getSum('c', tempCount, TI);
			}
			//cout<<"\t\t\t-> got the pixel information"<<endl;
			//<STEP 3>
			// Check the touching status
			for(int tempCount = thWd+1; tempCount<wd-thWd+1; tempCount++){
				// Conditions : Checking to find out the probable segmentation location
				// CR(tempCount) == 1 : Number of crossing is 1
				// PI(tempCount)==1 : Number of pixel is 1
				// (wd-tempCount)/ht > 0.4 : The following unit is not like aa-kar
				//cout<<"\t\t\t\tAttempt to find out the probable segmentation location"<<endl;
				if(CR[tempCount] == 1 && PI[tempCount]==1 && (float)(wd-tempCount)/ht > 0.45){
					// Check : If the unit likely to aa-kar then check its
					// characteristics (Eliminate problems in ta, va)
					//cout<<"\t\t\t\t\tCheck : If the unit likely to aa-kar.."<<endl;
					if((float)(tempCount-1)/ht < 0.45){
						//cout<<"\t\t\t\t\tCondition TRUE"<<endl;
						bytearray tempTI;
						cropingImage(TI, tempTI, 0, TI.dim(0)-1, 0, tempCount-1);
						//cout<<"\t\t\t\t\t\tImage Cropped.."<<endl;
						int vBarLoc = getVerticalBar(tempTI);
						//cout<<"\t\t\t\t\t\tGot Vertical Bar.."<<endl;
						if(vBarLoc > 0){
							TPoints.push_back(tempCount);
						}
						else{
							TPoints.push_back(tempCount);
						}
					}else if(CR[tempCount] == 1 && PI[tempCount]==2 && ( (PI[tempCount-1] >= (float)ht* 0.7) | (PI[tempCount-2] >= (float)ht* 0.7) ) && (wd-tempCount)/(float)ht > 0.4){
						//cout<<"\t\t\t\t\tCondition FALSE"<<endl;
						int crLoc = 1;
						// get the crossing location
						for(int crLocCount = 0; crLocCount<ht; crLocCount++){
							if(TI(crLocCount, tempCount)==0){
								crLoc = crLocCount;
								break;
							}
						}
						
						if((float)crLoc/ht < 0.25){
							int wdOfPrevUnit;
							// Get the width of the previous unit
							if(TPoints.empty()){
								wdOfPrevUnit = tempCount - 1;
							}else{
								wdOfPrevUnit = tempCount - TPoints[TPoints.size() -1];
							}
							
							// Get the width-Vs-height ratio of the previous unit
							float wdVshtRatio = (float)wdOfPrevUnit / ht;
							
							if(wdVshtRatio < 0.4){
								TPoints.push_back(tempCount);
							}
						} // if(crLoc/ht < 0.25)
						else{
							TPoints.push_back(tempCount);
						} // if(crLoc/ht < 0.25)
						
					} // if(((float)tempCount-1)/ht < 0.45)
					
				} // if(CR[tempCount] == 1 && PI[tempCount]==1 && ((float)wd-tempCount)/ht > 0.45)
				
			} // for(int tempCount = thWd+1;tempCount<wd-thWd+1;tempCount++)
			
		} // if(TPoints.empty())
		
		return TPoints;
	}
	
	// function for obtaining units from an array of touching points
	// input / parameter -> TPoints : Array of the touching points
	// Output -> Units : Units extracted
	
	vector<vector<int> > getUnitsFromTouchingPoints(vector<int> TPoints, colib::bytearray &SI){
		
		vector<vector<int> > Units;
		
		// get the size
		//int ht = SI.dim(0);
		int wd =  SI.dim(0);
		
		int thValPointDiff = 3; // Threshold value for the gap between points
		
		// get the number of points
		int numOfPoints = TPoints.size();
		
		// Re-build the point array using start and end point
		
		// Initialize the point related data structures
		vector<int> Points;
		
		// Set the starting point
		if(TPoints[1] - 1 >= thValPointDiff){
			Points.push_back(1);
		}else{
			TPoints.at(0) = 1;
		}
		//cout<<"-> Set the starting point"<<endl;
		
		// copy the other points from TPoints
		for(int numOfPointsCounter = 0; numOfPointsCounter < numOfPoints-1; numOfPointsCounter++){
			if(TPoints[numOfPointsCounter+1] - TPoints[numOfPointsCounter] > thValPointDiff){
				Points.push_back(TPoints[numOfPointsCounter]);
			}
		}
		//cout<<"-> copy the other points from TPoints"<<endl;
		
		// Set the end points
		if(wd - TPoints[numOfPoints-1] > thValPointDiff){
			Points.push_back(TPoints[numOfPoints-1]);
			Points.push_back(wd);
		}else{
			Points.push_back(wd);
		}
		//cout<<"-> Set the end point"<<endl;
		
		// Reset the number of points
		numOfPoints = Points.size();
		
		// Initialize the point counter
		int countPoint = -1;
		vector<int> AcPoints;
		//cout<<"-> Initialized the point counter"<<endl;
		
		// Set the Initial points in the Actual Point array
		countPoint++;
		AcPoints.push_back(Points[0]);
		
		// check the validity of the unit using the bottom position
		bytearray tempSI;
		cropingImage(SI, tempSI, 0, SI.dim(0)-1, Points[0], Points[1]);
		bool validity = checkValidityUsingBottom(tempSI);
		//cout<<"-> check the validity of the unit using the bottom position.."<<endl;
		
		if(validity){
			countPoint++;
			AcPoints.push_back(Points[countPoint]);
		}
		
		// Check the validity of the rest of the points and
		// take necessary actions (add / seperate)
		int startPoint , endPoint;
		for(int numOfPointsCounter=1 ; numOfPointsCounter<numOfPoints-1; numOfPointsCounter++){
			startPoint = Points[numOfPointsCounter]+1;
			endPoint = Points[numOfPointsCounter+1];
			
			// check the validity of the unit using the
			// bottom position
			cropingImage(SI, tempSI, 0, SI.dim(0)-1, startPoint, endPoint);
			bool validity = checkValidityUsingBottom(tempSI);
			
			if(validity){
				countPoint++;
				AcPoints.push_back(endPoint);
			}else{
				AcPoints.pop_back();
				AcPoints.push_back(endPoint);
			}
		}
		
		// Build the actual Units vector
		vector<int> tempUnit;
		
		// for the first unit
		tempUnit.push_back(AcPoints[0]);
		tempUnit.push_back(AcPoints[1]-1);
		Units.push_back(tempUnit);
		tempUnit.clear();
		
		// Proceed only if there is more than 2 points (means more than 1 unit)
		if(countPoint > 2){
			// Now print the units
			for(int numOfPointsCounter=1 ; numOfPointsCounter<countPoint-2; numOfPointsCounter++){
				tempUnit.push_back(AcPoints[numOfPointsCounter]+1);
				tempUnit.push_back(AcPoints[numOfPointsCounter+1]-1);
				Units.push_back(tempUnit);
				tempUnit.clear();
			}
			
			// for the last unit
			tempUnit.push_back(AcPoints[countPoint-1]+1);
			tempUnit.push_back(AcPoints[countPoint]);
			Units.push_back(tempUnit);
			tempUnit.clear();
		}
		
		/// return
		return Units;
	}
	
	// function for checking the validity of a unit using bottom location
	// input / parameter -> I : Image of the touching character
	// validity : true and false
	bool checkValidityUsingBottom(colib::bytearray &I){
		bool validity;
		
		// get the size
		int r = I.dim(0);
		int c = I.dim(1);
		int loc = 0;
		
		for(int i=r-1; i>-1 ; i--){
			if((c - getSum('r',i,I)) != 0){
				loc = i;
				break;
			}
		}
		
		if(loc > 2*(float)r/3){
			validity = true;
		}else{
			validity = false;
		}
		
		return validity;
	}
	
	// CRBLP <31-07-2008>
	void segmentUnits(colib::intarray &result_segmentation, colib::bytearray &orig_image){
		// Build Matlab format Image
		bytearray matlab_page_image;
		makeProperBinaryMatLabFormatImage(orig_image, matlab_page_image);		
		
		// Get the word boundaries
		intarray word_info;
		getWords(matlab_page_image, word_info);
		
		vector<UnitInformation> LineWordsInfo;
		
		//for(int x=2; x<3; x++){
		for(int x=0; x<word_info.dim(0); x++){
			intarray units_info;
			
			getUnits(matlab_page_image, units_info, word_info(x, 0), word_info(x, 1));
			
			int numOfUnits = units_info.dim(0);
			//cout<<"numOfUnits :: "<<numOfUnits<<endl;
			int numOfElementsPerUnit = units_info.dim(1);
			
			// sort the units info by left boundary information
			int** sortedUnitsInfo = allocate2DArray(numOfUnits, numOfElementsPerUnit);
			sortByRows(units_info, sortedUnitsInfo, numOfUnits, numOfElementsPerUnit, 2);
			
			// individual units
			int yy = word_info(x, 0);
			for(int y=0; y<numOfUnits; y++){
				UnitInformation uinfo;				
				
				// set the actual height
				uinfo.height = sortedUnitsInfo[y][4];
				
				// set the actual width
				uinfo.width = sortedUnitsInfo[y][5];
				
				// set top info
				uinfo.top = sortedUnitsInfo[y][0];
				
				// set bottom info
				uinfo.bottom = sortedUnitsInfo[y][1];
				
				// set left info
				uinfo.left = yy + sortedUnitsInfo[y][2];
				
				// set right info
				uinfo.right = yy + sortedUnitsInfo[y][3];
				
				// set the matraa starting position
				uinfo.matraaSrartLoc = sortedUnitsInfo[y][6];
				
				// set the matraa ending positionrig
				uinfo.matraaEndLoc = sortedUnitsInfo[y][7];
				
				// set the line number
				uinfo.lineNumber = 1;
				
				// set the word number
				uinfo.wordNumber = x;
				
				// set the unit number
				uinfo.unitNumber = y;
				
				// set the zone info
				uinfo.zoneNumber = sortedUnitsInfo[y][8];
				
				// set the shadow units location as -1
				uinfo.locOfShadowUnit = -1;
				
				LineWordsInfo.push_back(uinfo);
			}
			
			// free memory
			delete2DArray(sortedUnitsInfo, numOfUnits);
		}
		
		/*cout<<"Extracted LineWords Information :: "<<endl;
		for(int lnwC = 0; lnwC<LineWordsInfo.size(); lnwC++){
			UnitInformation uinfo = LineWordsInfo[lnwC];
			cout<<lnwC<<"  - "<<"Top - "<<uinfo.top<<", bottom - "<<uinfo.bottom<<", left - "<<uinfo.left<<", right - "<<uinfo.right<<", zone - "<<uinfo.zoneNumber<<endl;
		}*/
		
		//cout<<" :| Here"<<endl;
		// Get Joining Error Free Units
		vector<UnitInformation> LineUnitsInfo;
		LineUnitsInfo = getJoiningErrorFreeUnits(matlab_page_image, LineWordsInfo);
		LineWordsInfo.clear();
		//cout<<" :) Here"<<endl;
		
		/*cout<<"Extracted LineUnits Information :: "<<endl;
		for(int lnwC = 0; lnwC<LineUnitsInfo.size(); lnwC++){
			UnitInformation uinfo = LineUnitsInfo[lnwC];
			cout<<lnwC<<"  - "<<"Top - "<<uinfo.top<<", bottom - "<<uinfo.bottom<<", left - "<<uinfo.left<<", right - "<<uinfo.right<<", zone - "<<uinfo.zoneNumber<<endl;
		}*/
		
		//cout<<" :| Here"<<endl;
		// Get Joining Error Free Units
		vector<UnitInformation> AllUnitsInfo;
		AllUnitsInfo = getSplittingErrorFreeUnitsOfALine(LineUnitsInfo);
		LineUnitsInfo.clear();
		//cout<<" :) Here"<<endl;
		
		/*cout<<"Extracted AllUnitsInfo Information :: "<<endl;
		for(int lnwC = 0; lnwC<AllUnitsInfo.size(); lnwC++){
			UnitInformation uinfo = AllUnitsInfo[lnwC];
			cout<<lnwC<<"  - "<<"Top - "<<uinfo.top<<", bottom - "<<uinfo.bottom<<", left - "<<uinfo.left<<", right - "<<uinfo.right<<", zone - "<<uinfo.zoneNumber<<endl;
		}*/
		
		//cout<<"Number of units ::: "<<AllUnitsInfo.size()<<endl;
		
		////////////////// assign color to each unit ////////////////////////////
		int colorInfo = 0;
		int numOfunit = AllUnitsInfo.size();
		
		// For the first unit
		if(AllUnitsInfo[0].zoneNumber == 1){
			AllUnitsInfo[0].colorNumber = colorInfo+1;
		}else{
			colorInfo++;
			AllUnitsInfo[0].colorNumber = colorInfo;
		}
		
		// For middle units
		for(int wInd = 1; wInd<numOfunit-1; wInd++){
			if(AllUnitsInfo[wInd].zoneNumber == 1){
				if(AllUnitsInfo[wInd-1].left <= AllUnitsInfo[wInd].left && AllUnitsInfo[wInd-1].right >= AllUnitsInfo[wInd].right){
					// Means the upper unit is right in top of the previous
					// unit. So must be same color as the previous unit
					AllUnitsInfo[wInd].colorNumber = colorInfo;
				}else if(AllUnitsInfo[wInd+1].left < AllUnitsInfo[wInd].right && AllUnitsInfo[wInd+1].right >= AllUnitsInfo[wInd].right){
					// Means the right of upper unit is in the middle of the
					// next middle zone unit
					AllUnitsInfo[wInd].colorNumber = colorInfo + 1;
				}else if(AllUnitsInfo[wInd+1].left > AllUnitsInfo[wInd].left && AllUnitsInfo[wInd+1].right > AllUnitsInfo[wInd].right && (AllUnitsInfo[wInd+1].width/AllUnitsInfo[wInd+1].height > 0.4)){
					// Means the upper unit is completely detached from the
					// next unit. Special condition for ref
					colorInfo++;
					AllUnitsInfo[wInd].colorNumber = colorInfo;
				}else{
					AllUnitsInfo[wInd].colorNumber = colorInfo+1;
				}
			}else{
				colorInfo++;
				AllUnitsInfo[wInd].colorNumber = colorInfo;
			}
		}
		
		// For the last unit
		if(AllUnitsInfo[numOfunit-1].zoneNumber ==1){
			AllUnitsInfo[numOfunit-1].colorNumber = colorInfo+1;
		}else{
			colorInfo++;
			AllUnitsInfo[numOfunit-1].colorNumber = colorInfo;
		}
    				
		//// dealing with building color information
		//cout<<"dealing with building color information..."<<endl;
		intarray segmentation;
		
		segmentation.resize(matlab_page_image.dim(0),matlab_page_image.dim(1));
		for(int i=0;i<matlab_page_image.dim(0);i++){
			for(int j=0;j<matlab_page_image.dim(1);j++){
				segmentation(i,j)=0xffffffff;
			}
		}
		
		//cout<<" Preparing color image ..."<<endl;
		
		int top=0;
		int bottom=0;
		int left_=0;
		int right_=0;
		
		int unitCount = AllUnitsInfo.size();
		
		for(int index = 0; index<unitCount; index++){
			top = AllUnitsInfo[index].top;
			bottom = AllUnitsInfo[index].bottom;
			left_ = AllUnitsInfo[index].left;
			right_ = AllUnitsInfo[index].right;
			
			//cout<<index<<"  - "<<"Top - "<<top<<", bottom - "<<bottom<<", left - "<<left_<<", right - "<<right_<<", zone - "<<AllUnitsInfo[index].zoneNumber<<endl;
			for(int i=top;i<=bottom;i++){
				for(int j=left_;j<=right_;j++){
					if(matlab_page_image(i,j)==0){
						//segmentation(i,j)=index+1;
						segmentation(i,j)=AllUnitsInfo[index].colorNumber;
					}
				}
			}
		}
		
		//cout<<" renumber_labels ..."<<endl;
		renumber_labels(segmentation,1);
		
		//cout<<" manual renumbering  ..."<<endl;
		// manual renumbering .... (1->0) 
		for(int i=0;i<segmentation.dim(0);i++){
			for(int j=0;j<segmentation.dim(1);j++){
				if(segmentation(i,j)==1)
					segmentation(i,j)=0;
			}
		}
		
		// Return back to OCROPUS image format
		//cout<<" Returning back to OCROPUS image image format  ..."<<endl;
		result_segmentation.resize(segmentation.dim(1), segmentation.dim(0));
		
		int yy = result_segmentation.dim(1) - 1;
		
		for(int x=0; x<result_segmentation.dim(0); x++){
			for(int y=0; y<result_segmentation.dim(1); y++) {
				result_segmentation(x, y) = segmentation((yy-y), x); 
			}
		}
		
		//cout<<" make_line_segmentation_white  ..."<<endl;
		make_line_segmentation_white(result_segmentation);
		
		//cout<<"setting line number  ..."<<endl;
		set_line_number(result_segmentation, 1);
	}
	// CRBLP <31-07-2008>
	
	// CRBLP <20-08-2008>
	void segmentUnitsOfBlockImage(colib::intarray &result_segmentation, colib::bytearray &orig_image){
		// Build Matlab format Image
		bytearray matlab_page_image;
		makeProperBinaryMatLabFormatImage(orig_image, matlab_page_image);		
		
		// segmentation
		intarray segmentation;
		
		segmentation.resize(matlab_page_image.dim(0),matlab_page_image.dim(1));
		for(int i=0;i<matlab_page_image.dim(0);i++){
			for(int j=0;j<matlab_page_image.dim(1);j++){
				segmentation(i,j)=0xffffffff;
			}
		}
		
		//// Extract the text lines and relevant information
		intarray Lines;
		getLines(matlab_page_image, Lines);
		
		//// for each Text Line do further processing
		int numOfline = Lines.dim(0);
		/*for(int i = 0 ; i<numOfline; i++){
			cout<<"Line [ "<<i<<" ] -> "<<Lines(i, 0)<<", "<<Lines(i, 1)<<endl;
		}*/
		//cout<<"Number of lines in the image: "<<numOfline<<endl;
		
		int thValLineHt = 5; // will be modified later
		int lineHt;
		//for(int i = 32 ; i<33; i++){
		for(int i = 0 ; i<numOfline; i++){
			//cout<<"Line [ "<<i<<" ] -> "<<Lines(i, 0)<<", "<<Lines(i, 1)<<endl;
			lineHt = Lines(i, 1) - Lines(i, 0) + 1;
			
			if(lineHt <= thValLineHt){
				continue;
			}
			bytearray Line;
			cropingImage(matlab_page_image, Line, Lines(i, 0), Lines(i, 1), 0, matlab_page_image.dim(1)-1);
			
			// Get the word boundaries
			intarray word_info;
			getWords(Line, word_info);
			//cout<<"Number of words : "<<word_info.dim(0)<<endl;
			vector<UnitInformation> LineWordsInfo;
			
			/*for(int x=0; x<word_info.dim(0); x++){
				cout<<"Word Number [ "<<x+1<<" ] -> < "<<word_info(x, 0)<<", "<<word_info(x, 1)<<endl;
			}*/
			
			//for(int x=31; x<32; x++){
			for(int x=0; x<word_info.dim(0); x++){
				//cout<<"Word Number : "<<x+1<<endl;
				intarray units_info;
				
				getUnits(Line, units_info, word_info(x, 0), word_info(x, 1));
				//cout<<"Here ... "<<endl;
				int numOfUnits = units_info.dim(0);
				//cout<<"numOfUnits :: "<<numOfUnits<<endl;
				int numOfElementsPerUnit = units_info.dim(1);
				
				// sort the units info by left boundary information
				int** sortedUnitsInfo = allocate2DArray(numOfUnits, numOfElementsPerUnit);
				sortByRows(units_info, sortedUnitsInfo, numOfUnits, numOfElementsPerUnit, 2);
				
				// individual units
				int yy = word_info(x, 0);
				for(int y=0; y<numOfUnits; y++){
					UnitInformation uinfo;				
					
					// set the actual height
					uinfo.height = sortedUnitsInfo[y][4];
					
					// set the actual width
					uinfo.width = sortedUnitsInfo[y][5];
					
					// set top info
					uinfo.top = sortedUnitsInfo[y][0];
					
					// set bottom info
					uinfo.bottom = sortedUnitsInfo[y][1];
					
					// set left info
					uinfo.left = yy + sortedUnitsInfo[y][2];
					
					// set right info
					uinfo.right = yy + sortedUnitsInfo[y][3];
					
					// set the matraa starting position
					uinfo.matraaSrartLoc = sortedUnitsInfo[y][6];
					
					// set the matraa ending positionrig
					uinfo.matraaEndLoc = sortedUnitsInfo[y][7];
					
					// set the line number
					uinfo.lineNumber = 1;
					
					// set the word number
					uinfo.wordNumber = x;
					
					// set the unit number
					uinfo.unitNumber = y;
					
					// set the zone info
					uinfo.zoneNumber = sortedUnitsInfo[y][8];
					
					// set the shadow units location as -1
					uinfo.locOfShadowUnit = -1;
					
					LineWordsInfo.push_back(uinfo);
				}
				
				// free memory
				delete2DArray(sortedUnitsInfo, numOfUnits);
			}

			/*cout<<"Extracted LineWords Information :: "<<endl;
			for(int lnwC = 0; lnwC<LineWordsInfo.size(); lnwC++){
			UnitInformation uinfo = LineWordsInfo[lnwC];
			cout<<lnwC<<"  - "<<"Top - "<<uinfo.top<<", bottom - "<<uinfo.bottom<<", left - "<<uinfo.left<<", right - "<<uinfo.right<<", zone - "<<uinfo.zoneNumber<<endl;
			}*/
			
			//cout<<" :| Here"<<endl;
			// Get Joining Error Free Units
			vector<UnitInformation> LineUnitsInfo;
			LineUnitsInfo = getJoiningErrorFreeUnits(Line, LineWordsInfo);
			LineWordsInfo.clear();
			//cout<<" :) Here"<<endl;
			
			/*cout<<"Extracted LineUnits Information :: "<<endl;
			for(int lnwC = 0; lnwC<LineUnitsInfo.size(); lnwC++){
			UnitInformation uinfo = LineUnitsInfo[lnwC];
			cout<<lnwC<<"  - "<<"Top - "<<uinfo.top<<", bottom - "<<uinfo.bottom<<", left - "<<uinfo.left<<", right - "<<uinfo.right<<", zone - "<<uinfo.zoneNumber<<endl;
			}*/
			
			//cout<<" :| Here"<<endl;
			// Get Joining Error Free Units
			vector<UnitInformation> AllUnitsInfo;
			AllUnitsInfo = getSplittingErrorFreeUnitsOfALine(LineUnitsInfo);
			LineUnitsInfo.clear();
			//cout<<" :) Here"<<endl;
			
			/*cout<<"Extracted AllUnitsInfo Information :: "<<endl;
			for(int lnwC = 0; lnwC<AllUnitsInfo.size(); lnwC++){
			UnitInformation uinfo = AllUnitsInfo[lnwC];
			cout<<lnwC<<"  - "<<"Top - "<<uinfo.top<<", bottom - "<<uinfo.bottom<<", left - "<<uinfo.left<<", right - "<<uinfo.right<<", zone - "<<uinfo.zoneNumber<<endl;
			}*/
			
			// cout<<"Number of units ::: "<<AllUnitsInfo.size()<<endl;

			////////////////// assign color to each unit ////////////////////////////
			int colorInfo = 0;
			int numOfunit = AllUnitsInfo.size();
			
			// For the first unit
			if(AllUnitsInfo[0].zoneNumber == 1){
				AllUnitsInfo[0].colorNumber = colorInfo+1;
			}else{
				colorInfo++;
				AllUnitsInfo[0].colorNumber = colorInfo;
			}
			
			// For middle units
			for(int wInd = 1; wInd<numOfunit-1; wInd++){
				if(AllUnitsInfo[wInd].zoneNumber == 1){
					if(AllUnitsInfo[wInd-1].left <= AllUnitsInfo[wInd].left && AllUnitsInfo[wInd-1].right >= AllUnitsInfo[wInd].right){
						// Means the upper unit is right in top of the previous
						// unit. So must be same color as the previous unit
						AllUnitsInfo[wInd].colorNumber = colorInfo;
					}else if(AllUnitsInfo[wInd+1].left < AllUnitsInfo[wInd].right && AllUnitsInfo[wInd+1].right >= AllUnitsInfo[wInd].right){
						// Means the right of upper unit is in the middle of the
						// next middle zone unit
						AllUnitsInfo[wInd].colorNumber = colorInfo + 1;
					}else if(AllUnitsInfo[wInd+1].left > AllUnitsInfo[wInd].left && AllUnitsInfo[wInd+1].right > AllUnitsInfo[wInd].right && (AllUnitsInfo[wInd+1].width/AllUnitsInfo[wInd+1].height > 0.4)){
						// Means the upper unit is completely detached from the
						// next unit. Special condition for ref
						colorInfo++;
						AllUnitsInfo[wInd].colorNumber = colorInfo;
					}else{
						AllUnitsInfo[wInd].colorNumber = colorInfo+1;
					}
				}else{
					colorInfo++;
					AllUnitsInfo[wInd].colorNumber = colorInfo;
				}
			}
			
			// For the last unit
			if(AllUnitsInfo[numOfunit-1].zoneNumber ==1){
				AllUnitsInfo[numOfunit-1].colorNumber = colorInfo+1;
			}else{
				colorInfo++;
				AllUnitsInfo[numOfunit-1].colorNumber = colorInfo;
			}
			
			//// dealing with building color information
			//cout<<"dealing with building color information..."<<endl;
			// cout<<" Preparing color image ..."<<endl;
			
			int top=0;
			int bottom=0;
			int left_=0;
			int right_=0;
			
			int unitCount = AllUnitsInfo.size();
			
			for(int index = 0; index<unitCount; index++){
				top = Lines(i, 0) + AllUnitsInfo[index].top;
				bottom = Lines(i, 0) + AllUnitsInfo[index].bottom;
				//top = AllUnitsInfo[index].top;
				//bottom = AllUnitsInfo[index].bottom;
				left_ = AllUnitsInfo[index].left;
				right_ = AllUnitsInfo[index].right;
				
				//cout<<index<<"  - "<<"Top - "<<top<<", bottom - "<<bottom<<", left - "<<left_<<", right - "<<right_<<", zone - "<<AllUnitsInfo[index].zoneNumber<<endl;
				for(int i=top;i<=bottom;i++){
					for(int j=left_;j<=right_;j++){
						if(matlab_page_image(i,j)==0){
							segmentation(i,j)=AllUnitsInfo[index].colorNumber;
						}
					}
				}
			}
			
		} // for(int i = 0 ; i<numOfline; i++)
		
		
		//cout<<" renumber_labels ..."<<endl;
		renumber_labels(segmentation,1);
		
		//cout<<" manual renumbering  ..."<<endl;
		// manual renumbering .... (1->0) 
		for(int i=0;i<segmentation.dim(0);i++){
			for(int j=0;j<segmentation.dim(1);j++){
				if(segmentation(i,j)==1)
					segmentation(i,j)=0;
			}
		}
		
		// Return back to OCROPUS image format
		//cout<<" Returning back to OCROPUS image image format  ..."<<endl;
		result_segmentation.resize(segmentation.dim(1), segmentation.dim(0));
		
		int yy = result_segmentation.dim(1) - 1;
		
		for(int x=0; x<result_segmentation.dim(0); x++){
			for(int y=0; y<result_segmentation.dim(1); y++) {
				result_segmentation(x, y) = segmentation((yy-y), x); 
			}
		}
		
		//cout<<" make_line_segmentation_white  ..."<<endl;
		make_line_segmentation_white(result_segmentation);
		
		//cout<<"setting line number  ..."<<endl;
		set_line_number(result_segmentation, 1);
	}
	// CRBLP <20-08-2008>
	
	// CRBLP <18-07-08>
	void convert_to_color_segmented_image(colib::intarray &result_segmentation, colib::bytearray &orig_image){
		bytearray Timage;
		copy(Timage, orig_image);
		
		// make the image proper binary
		make_page_binary_and_black(Timage);
		for(int row=0; row<Timage.dim(0); row++){
			for(int col=0;col<Timage.dim(1);col++){
				if(Timage(row, col)!=0)
					Timage(row, col) = 0;
				else
					Timage(row, col) = 1;
			}
		}
		
		// Build Matlab format Image
		bytearray matlab_page_image;
		int col=orig_image.dim(0);
		int row=orig_image.dim(1);
		
		matlab_page_image.resize(row, col);
		
		int xx = matlab_page_image.dim(0) - 1;
		for(int x=0; x < matlab_page_image.dim(0); x++){
			for(int y=0; y<matlab_page_image.dim(1); y++){
				matlab_page_image(x, y) = Timage(y, (xx-x)); 
			}
		}
		
		
		// Get the word boundaries
		intarray word_info;
		getWords(matlab_page_image, word_info);
		
		int line_units_info[100][4];
		int unitCount = -1;
		
		for(int x=0; x<word_info.dim(0); x++){
			intarray units_info;
			//cout<<"Word Boundary - < "<<word_info(x, 0)<<", "<<word_info(x, 1)<<" >"<<endl;
			//cout<<"Word - < "<<x+1<<" >"<<endl;
			getUnits(matlab_page_image, units_info, word_info(x, 0), word_info(x, 1));
			
			int numOfUnits = units_info.dim(0);
			int numOfElementsPerUnit = units_info.dim(1);
			
			// sort the units info by left boundary information
			int** sortedUnitsInfo = allocate2DArray(numOfUnits, numOfElementsPerUnit);
			sortByRows(units_info, sortedUnitsInfo, numOfUnits, numOfElementsPerUnit, 2);
			
			// individual units
			int yy = word_info(x, 0);
			for(int y=0; y<numOfUnits; y++){
				unitCount++;
				
				//cout<<"Unit ["<<y<<"] -> wd ["<<yy + units_info(y,2)<<", "<<yy + units_info(y,3)<<"]"<<", ht ["<<units_info(y,0)<<", "<<units_info(y,1)<<"]"<<endl;
				
				// top info
				line_units_info[unitCount][0] = sortedUnitsInfo[y][0];
				// bottom info
				line_units_info[unitCount][1] = sortedUnitsInfo[y][1];
				// left info
				line_units_info[unitCount][2] = yy + sortedUnitsInfo[y][2];
				// right info
				line_units_info[unitCount][3] = yy + sortedUnitsInfo[y][3];
			}
			
			// free memory
			delete2DArray(sortedUnitsInfo, numOfUnits);
		}
		
		intarray segmentation;
		intarray csegmentation;
		
		segmentation.resize(matlab_page_image.dim(0),matlab_page_image.dim(1));
		csegmentation.resize(matlab_page_image.dim(0),matlab_page_image.dim(1));
		for(int i=0;i<matlab_page_image.dim(0);i++)
		for(int j=0;j<matlab_page_image.dim(1);j++){
			segmentation(i,j)=0xffffffff;
			csegmentation(i,j)=16777215;
		}
		
		//int width = matlab_page_image.dim(1);
		
		int top=0;
		int bottom=0;
		int left_=0;
		int right_=0;
		
		
		for(int index = 0; index<=unitCount; index++){
			top = line_units_info[index][0];
			bottom = line_units_info[index][1];
			left_ = line_units_info[index][2];
			right_ = line_units_info[index][3];
			
			//cout<<"Top - "<<top<<", bottom - "<<bottom<<", left - "<<left_<<", right - "<<right_<<endl;
			for(int i=top;i<=bottom;i++){
				for(int j=left_;j<=right_;j++){
					if(matlab_page_image(i,j)==0){
						segmentation(i,j)=index+1;
						csegmentation(i,j)=index+1;
					}
				}
			}
		}
		
		/*ofstream SaveFile("csegmentation.txt");
		for(int row=0; row<segmentation.dim(0); row++){
		for(int col=0;col<csegmentation.dim(1);col++){
		SaveFile <<(int)csegmentation(row,col)<<" ";
		}
		SaveFile <<endl;
		}
		SaveFile.close();*/
		
		renumber_labels(segmentation,1);
		
		// manual renumbering .... (1->0) 
		for(int i=0;i<segmentation.dim(0);i++){
			for(int j=0;j<segmentation.dim(1);j++){
				if(segmentation(i,j)==1)
					segmentation(i,j)=0;
			}
		}
		
		// Return back to OCROPUS image format
		
		result_segmentation.resize(segmentation.dim(1), segmentation.dim(0));
		
		int yy = result_segmentation.dim(1) - 1;
		
		for(int x=0; x<result_segmentation.dim(0); x++){
			for(int y=0; y<result_segmentation.dim(1); y++) {
				result_segmentation(x, y) = segmentation((yy-y), x); 
			}
		}
		
		make_line_segmentation_white(result_segmentation);
		set_line_number(result_segmentation, 1);
	}
	
	// CRBLP <18-07-08>
	
	//CRBLP_code
	void crblpCharseg(colib::intarray &result_segmentation,colib::bytearray &orig_image){
		bytearray image;
		copy(image, orig_image);
		
		make_page_binary_and_black(image);
		intarray segmentation;
		
		//////////////////////////////mhasnat-n-SSS/////////////////////////////
		bytearray Simage;
		segmentation.resize(image.dim(0),image.dim(1));
		int col=orig_image.dim(0);
		int row=orig_image.dim(1);
		
		Simage.resize(row, col);
		
		//coping orig_image to image
		// copy the information of the pixels into 1-D array
		int tempInfo[col*row];
		int tempCount = 0;
		for(int i=0;i<col;i++){
			for(int j=0;j<row;j++){
				tempInfo[tempCount] = orig_image(i,j);
				tempCount++;
			}
		}
		
		tempCount = 0;
		
		for (int i=col-1;i>=0;i--){
			for(int j=row-1;j>=0;j--){
				Simage(j,i) = tempInfo[tempCount];
				tempCount++;
			}
		}
		
		//makeing image binary and black
		make_page_binary_and_black(Simage);
		
		/*ofstream SaveFile("Simage.txt");
		for(int row=0; row<Simage.dim(0); row++){
		for(int col=0;col<Simage.dim(1);col++){
		SaveFile <<(int)Simage(row,col)<<" ";
		}
		SaveFile <<endl;
		}
		SaveFile.close();*/
		
		//get the top
		int top=1;
		int sum = 0;
		for(int i=0;i<row;i++){
			sum = 0;
			for (int j = 0; j<col; j++){
				sum = sum + Simage(i,j);
			}
			
			if(sum != 0){
				top = i;
				break;
			}
		}
		
		
		//get the bottom
		int bottom=1;
		for(int i=row-1;i>=0;i--){
			sum = 0;
			for (int j = 0; j<col; j++)
				sum = sum + Simage(i,j);
			
			if(sum != 0){
				bottom = i;
				break;
			}
		}
		
		//crop image
		intarray row_corped_image;
		int croped_row=(bottom-top)+1;
		row_corped_image.resize(croped_row,col);
		for(int i=top,ni=0;i<bottom;i++,ni++)
		for(int j=0,nj=0;j<col;j++,nj++){
			row_corped_image(ni,nj)=Simage(i,j);
		}
		
		// Reset the size of the Image of Interest
		int height = croped_row;
		int width = col;
		
		
		// calculate the horizontal histogram of the cropped image
		int hHistogram[height];
		int maxval=0;
		int index=0;
		for(int i=0;i<height;i++){
			sum = 0;
			for(int j=0;j<width;j++){
				if(row_corped_image(i,j))
					sum++;
			}
			
			if(sum>maxval){
				maxval=sum;
				index=i;
			}
			hHistogram[i] = sum;
		}
		
		//fixing the start and end index
		int startIndex;
		int endIndex;
		if(index<=0){
			startIndex=0;
			endIndex=index+4;
		}else{
			startIndex=index-2;
			endIndex=index+2;
		}
		
		// removing the matra
		for(int i=startIndex;i<=endIndex;i++){
			for(int j=0;j<width;j++){
				if(row_corped_image(i,j)==255)
					row_corped_image(i,j)=0;
			}
		}
		
		
		//int matraStartLocation=top+startIndex;
		//int matraEndLocation=top+endIndex;
		
		// calculate the vertical histogram of the cropped image
		int vHistogram[width];
		//cout<<"Histogram - ";
		for(int i=width-1;i>=0;i--){
			sum = 0;
			//cout<<"[ "<<width-i-1<<" ] - ";
			for(int j=0;j<height;j++){
				//cout<<(int)row_corped_image(j,i)<<" ";
				if(int(row_corped_image(j,i))==255){
					sum++;
				}
			}
			//cout<<", "<<sum;
			vHistogram[width-i-1] = sum;
		}
		//cout<<endl;
		
		//// Extract the boundary points
		int count = -1;
		int chracterBoundary[width];
		
		int startSearchIndex = 0; // Searching index for the vertical histogram
		
		// for the first non-zero column : special case when the first column histogram value is non-zero
		if(vHistogram[0]!=0){
			count = count + 1;
			chracterBoundary[count] = -1; // CB - Character Boundary
			startSearchIndex = 1;
		}
		// find all the zero valued location from the histogram
		for(int i = startSearchIndex;i<width;i++){
			if(vHistogram[i]==0){
				count = count + 1;
				chracterBoundary[count] = i; // CB - Character Boundary
			}
		}
		// for the last non-zero column : special case when the last column histogram value is non-zero
		if(vHistogram[width-1]!=0){
			count = count + 1;
			chracterBoundary[count] = width; // CB - Character Boundary
		}
		
		// find out the boundary points
		intarray PUnits;
		PUnits.resize(count,4);
		int left_=0;
		int right_=0;
		int thVal = 1;
		int count_ = -1;
		for(int i = 1;i<=count;i++){
			if((chracterBoundary[i] - chracterBoundary[i-1]) > thVal){
				count_ = count_ + 1;
				// left and right
				left_ = chracterBoundary[i-1] + 1;
				right_ = chracterBoundary[i] - 1;
				/// get the necessary info about the subimage or unit box
				//top
				PUnits(count_, 0) = top;
				// bottom
				PUnits(count_, 1) = bottom;
				// left
				PUnits(count_, 2) = left_;
				// right
				PUnits(count_, 3) = right_;
			}
		}
		
		segmentation.resize(Simage.dim(0),Simage.dim(1));
		for(int i=0;i<Simage.dim(0);i++)
		for(int j=0;j<Simage.dim(1);j++){
			segmentation(i,j)=0xffffffff;
		}
		
		for(int index = 0;index<=count_;index++){
			top = PUnits(index, 0);
			bottom = PUnits(index, 1);
			right_ = width - PUnits(index, 2);
			left_ = width - PUnits(index, 3);
			
			for(int i=top;i<=bottom;i++){
				for(int j=left_;j<=right_;j++){
					if(Simage(i,j)==255)
						segmentation(i,j)=4097+index;
				}
			}
		}
		
		renumber_labels(segmentation,1);
		
		// manual renumbering .... (1->0) 
		for(int i=0;i<segmentation.dim(0);i++){
			for(int j=0;j<segmentation.dim(1);j++){
				if(segmentation(i,j)==1)
					segmentation(i,j)=0;
			}
		}
		
		///// Re-organizing the segmentation as like this convention
		tempCount = 0;
		
		for (int i=col-1;i>=0;i--){
			for(int j=row-1;j>=0;j--){
				tempInfo[tempCount] = segmentation(j,i);
				tempCount++;
			}
		}
		
		result_segmentation.resize(col,row);
		tempCount = 0;
		for(int i=0;i<col;i++){
			for(int j=0;j<row;j++){
				result_segmentation(i, j) = tempInfo[tempCount];
				tempCount++;
			}
		}
		
		make_line_segmentation_white(result_segmentation);
		set_line_number(result_segmentation, 1);
	}
	//CRBLP_code
	
	void createTesseractFormatImage(intarray &I, bytearray &RI, intarray &SI){
		RI.resize(I.dim(0), I.dim(1));
		SI.resize(I.dim(0), I.dim(1));
		
		for(int row=0;row<I.dim(0);row++){
			for(int col=0; col<I.dim(1); col++){
				if(I(row,col)==16777215){
					RI(row, col) = 0;
					SI(row, col) = 16777215;
				}else{
					RI(row, col) = 255;
					SI(row, col) = 65537;
				}
			}
		}
	}
	
	void getUnits(bytearray &I, intarray &UnitsInfo, int start, int end){
		bytearray Word;
		Word.resize(I.dim(0), end-start+1);
		
		// Crop the word image
		for(int row=0;row<I.dim(0);row++){
			for(int col=start, c = 0; col<=end; col++, c++){
				Word(row, c) = I(row, col);
			}
		}
		
		// Data Structure : Units
		// 1. Top Index
		// 2. Bottom Index
		// 3. Left Index
		// 4. Right Index
		// 5. Image Height
		// 6. Image Width
		// 7. Matraa start Location
		// 8. Matraa end Location
		// 9. Zone Information ( 1 - Upper , 2 - Middle & 3 - Lower )
		
		
		// extract the proper boundary (top and bottom)
		int r=Word.dim(0);
		int c=Word.dim(1);
		
		vector<vector<int> > Units;
		int totalNumberOfUnits = 0;
		
		// get the top
		int top = 1;
		for(int i=0;i<r;i++){
			if((c - getSum('r',i,Word)) != 0){
				top = i;
				break;
			}
		}
		
		// get the bottom
		int bottom = r;
		for(int i=r-1;i>=0;i--){
			if((c - getSum('r',i,Word)) != 0){
				bottom = i;
				break;
			}
		}
		
		// =================================
		// Step 1: Locate the header line
		// =================================
		
		// get the proper Word boundary and assign it into other word
		bytearray TWord;
		cropingImage(Word, TWord, top, bottom, 0, Word.dim(1)-1);
		
		// <print the image>
		/*ofstream SaveFile("cropped_word_image.txt");
		for(int row=0; row<TWord.dim(0); row++){
		for(int col=0;col<TWord.dim(1);col++){
		SaveFile <<(int)TWord(row,col)<<" ";
		}
		SaveFile <<endl;
		}
		SaveFile.close();
		*/
		
		// get the size
		int r1=TWord.dim(0);
		int c1=TWord.dim(1);
		
		// if the size is less than a threshold value then we can omit that
		int thVal = 5;
		
		// find the peak index trhough run length histogram value
		int RLH[r1];
		rlhist(TWord,RLH);
		
		int size = sizeof(RLH)/sizeof(int);
		int inDx=getMaxIndx(RLH, size);
		
		// @cond-1: r1>thVal -> the size of the character is less than
		// threshold value
		// @cond-2: r1<2*c1  -> the height of the character is less than
		// twice the width
		// @cond-3: inDx<r1*3/7  -> the matraa location is on the upper half
		
		if(r1>thVal && r1<1.5*c1 && inDx<(r1*3)/7){
			
			int ed_index_up_zone;
			int st_index_ml_zone;
			
			getMatraaLocPH(TWord, ed_index_up_zone, st_index_ml_zone);
			/* from the observed output it is clear that in several cases few pixels remain in the row
			just after the matraa end location. Considering this situation we will take into account 
			1 row more as matraa. */
			st_index_ml_zone = st_index_ml_zone + 1;
    
			// cout<<"ed_index_up_zone :: "<<ed_index_up_zone<<", st_index_ml_zone :: "<<st_index_ml_zone<<endl;

			// locate the matraa start and end location
			int matraa_start_loc;
			int matraa_end_loc;
			if(inDx >= ed_index_up_zone && inDx <= st_index_ml_zone){
				matraa_start_loc = top + ed_index_up_zone + 1;
				matraa_end_loc   = top + st_index_ml_zone - 1;
				//cout<<"matraa_start_loc :: "<<matraa_start_loc<<", matraa_end_loc :: "<<matraa_end_loc<<endl;	
			}else{
				matraa_start_loc = top + inDx - 1;
				matraa_end_loc   = top + inDx + 1;
				//cout<<"matraa_start_loc :: "<<matraa_start_loc<<", matraa_end_loc :: "<<matraa_end_loc<<endl;
			}
			
			// ===================================================================
			// Step 2: Seperate character / symbol boxes of the image below the
			// header line
			// ===================================================================
			
			bytearray MWord;
			intarray LabeledWord;
			bytearray UpPart;
			bytearray UpEnvelope;
			
			cropingImage(Word, MWord, matraa_start_loc, bottom, 0,Word.dim(1)-1);
			cropingImage(Word, UpPart, matraa_start_loc, matraa_end_loc, 0, Word.dim(1)-1);
		
			// <<< Special Note : This Part of code will be enabled when bwlabel implementation is done>>>
			
			// Initialize the upper envelope with 0
			UpEnvelope.resize(UpPart.dim(0), UpPart.dim(1));
			
			for(int upr=0; upr<UpEnvelope.dim(0); upr++){
				for(int upc=0; upc<UpEnvelope.dim(1); upc++){
					UpEnvelope(upr, upc) = 1;
				}
			}
			
			// Extract the upper envelope of this UpPart
			for(int upc=0; upc<UpPart.dim(1); upc++){
				for(int upr=0; upr<UpPart.dim(0); upr++){
					if(UpPart(upr, upc) == 0){
						UpEnvelope(upr, upc) = 0;
						break;
					}
				}
			}
			
			/////// ######################################## ////
			float thValMatraa = 0.5;
			int numOfRowInUpPart = UpPart.dim(0);
			// get the connected components and extract the units
			int maxlabelNumber = getConnectedComponentInformation(UpEnvelope, LabeledWord);

			// Condition: split if there is more than one connected
			// component
			if(maxlabelNumber >= 1){
				for( int numOfCC = 1 ; numOfCC<=maxlabelNumber; numOfCC++){
					vector<point> points = find(numOfCC, LabeledWord);
					
					int totalPoints = points.size();
					int rInfo[totalPoints];
					int cInfo[totalPoints];
					
					for(int numOfPoint=0; numOfPoint<totalPoints; numOfPoint++){
						rInfo[numOfPoint] = points[numOfPoint].x;
						cInfo[numOfPoint] = points[numOfPoint].y;
					}
					
					int leftCC = getMinVal(cInfo, totalPoints);
					int rightCC = getMaxVal(cInfo, totalPoints);
					//int topCC = getMinVal(rInfo, totalPoints);
					int bottomCC = getMaxVal(rInfo, totalPoints);
					
					
					//int ht = bottomCC - topCC + 1;
					int width = rightCC - leftCC + 1;
					
					float ratioMatraa = float(width) / r1;
					
					if(ratioMatraa < thValMatraa && bottomCC > ceil(numOfRowInUpPart/2) ){
						// Keep it remain same
					}else{
						for (int mwi = 0; mwi<numOfRowInUpPart; mwi++){
							for (int mwj = leftCC; mwj<=rightCC; mwj++){
								MWord(mwi, mwj) = 1;
							}
						}
					}
					
				}
			}
			
			/*ofstream SaveFile("MWord.txt");
			for(int row=0; row<MWord.dim(0); row++){
				for(int col=0;col<MWord.dim(1);col++){
					SaveFile <<(int)MWord(row,col);
				}
				SaveFile <<endl;
			}
			SaveFile.close();*/
			
			// ***********************************************************************
			int r2=MWord.dim(0);
			int c2=MWord.dim(1);
			
			// calculate vertical histogram
			int VH[c2];
			for(int i=0;i<c2;i++){
				VH[i] = r2 - getSum('c',i,MWord);// VH: contains the vertical histogram result
			}
			
			// Extract the boundary points
			int count = -1;
			
			int CB[c2]; // Character Boundary array
			int startSearchIndex = 0; // Searching index for the vertical histogram
			
			// for the first non-zero column : special case when the first column
			// histogram value is non-zero
			//cout<<"VH :: ";
			if(VH[0]!=0){
				count = count + 1;
				CB[count] = -1; // CB - Character Boundary
				//	cout<<CB[count]<<", ";
				startSearchIndex = 1;
			}
			
			// find all the zero valued location from the histogram
			for( int i = startSearchIndex;i<c2;i++){
				if(VH[i]==0){
					count = count + 1;
					CB[count] = i; // CB - Character Boundary
					//		cout<<CB[count]<<", ";
				}
			}
			
			// for the last non-zero column : special case when the last column
			// histogram value is non-zero
			
			if(VH[c2-1]!=0){
				count = count + 1;
				CB[count] = c2; // CB - Character Boundary
				//	cout<<CB[count]<<", ";
			}
			//cout<<endl;
			
			// find out the boundary points
			thVal = 1;
			int count_ = -1;
			int left_, right_, width_;
			int unitTop, unitBottom, height_;
			bool validMZUnit;
			int thHtUnit = ceil(r2 / 7);
    
			vector<vector<int> > PUnits;
			
			for(int i = 1; i<=count; i++){
				if(CB[i] - CB[i-1] > thVal){
					count_++;
					
					vector<int> tempUnit;
					
					// left and right
					left_ = CB[i-1] + 1;
					right_ = CB[i] - 1;
					width_ = right_ - left_ + 1;
					
					// top and bottom
					unitTop = top + (ed_index_up_zone + 1);
					
					for(int botIndx = r2-1 ; botIndx>=0 ; botIndx--){
						int tempWd = getSumLR(MWord, botIndx, left_, right_);
						
						if(tempWd != width_){
							unitBottom = bottom - (r2 - botIndx) + 1;
							break;
						}
					}
					
					height_ = unitBottom - unitTop + 1;
					
					// check validity
					validMZUnit = true;
					
					if(height_ <thHtUnit)
						validMZUnit = false;
					
					// Insert if valid
					if(validMZUnit){ 
						
						// top
						tempUnit.push_back(unitTop);
						
						// bottom
						tempUnit.push_back(unitBottom);
						
						//cout<<"Top :: "<<tempUnit[0]<<", Bottom ::: "<<tempUnit[1]<<endl; 
						// left
						tempUnit.push_back(left_);
						
						// right
						tempUnit.push_back(right_);
						
						//cout<<"Left :: "<<tempUnit[2]<<", Right ::: "<<tempUnit[3]<<endl;
						
						// height
						tempUnit.push_back(height_);
						
						// width
						tempUnit.push_back(width_);
						
						// matraa start loc
						tempUnit.push_back(matraa_start_loc);
						
						// matraa end loc
						tempUnit.push_back(matraa_end_loc);
						
						// Zone Information
						tempUnit.push_back(2);
						
						PUnits.push_back(tempUnit);
						
						tempUnit.clear();
					}
				}
			}
			
			//cout<<"Number of Units "<<PUnits.size()<<endl;
			// <<< Special Note : This Part of code will be enabled when bwlabel implementation is done>>>
			///// Eliminate Matraa based joining (MBJ) error using projection histogram information | START 
			PUnits = eliminateMBJError(PUnits, Word);
			// cout<<"Size after eliminating MBJ errors :: "<<PUnits.size()<<endl;
			///// Eliminate Matraa based joining error using projection histogram information | END 
			
			///// ##### Eliminate Splitting Error | START ##### /////
			Units = eliminateSplittingError(PUnits);
			//cout<<"Size after eliminating Splitting errors :: "<<Units.size()<<endl;
			count_ = Units.size() - 1;
			///// ##### Eliminate Splitting Error | END ##### /////
			//cout<<"Size after "<<Units.size()<<endl;
			
			/*for(int i = 0; i<Units.size(); i++){
				cout<<"Left :: "<<Units[i][2]<<", Right ::: "<<Units[i][3]<<endl;
			}*/
			
			totalNumberOfUnits = count_;
			
			// ============================================
			///// Step 2: Seperate symbols of the top strip
			// ============================================
			
			int* tempArr = new int [count_+1];
			// copy height information of the units into tempArr
			for(int tmpCnt = 0; tmpCnt<=count_; tmpCnt++)
				tempArr[tmpCnt] = Units[tmpCnt][4];
				
			int thHtUpUnit = ceil(getMean(tempArr, count_+1) / 6);
			int matraaLength = st_index_ml_zone - ed_index_up_zone - 1;
			
			if(ed_index_up_zone+1 >= matraaLength){
				// Upper zone of the word
				bytearray UZWord;
				
				cropingImage(TWord, UZWord, 0,ed_index_up_zone,0,TWord.dim(1)-1);
				
				r2=UZWord.dim(0);
				c2=UZWord.dim(1);
				
				// calculate horizontal histogram
				int* HH = new int[r2];
				//cout<<"HH :: "<<endl;
				for(int i=0; i<r2; i++){
					HH[i] = c2 -getSum('r', i, UZWord); // HH: contains the horizontal histogram result
					//cout<<HH[i]<<endl;
				}
				
				int inDx = getMaxIndx(HH, r2);
				
				// free memory
				delete[] HH;
				
				if(inDx >= r2 - ceil(r2/8)){
					ed_index_up_zone = inDx - 1;
					
					// UZWord(inDx:r2, :) = 1;
					for(int i=inDx; i<r2; i++){
						for(int j=0; j<c2; j++){
							UZWord(i, j) = 1;
						}
					}
				}

				/*ofstream SaveFile("UZWord.txt");
				for(int row=0; row<UZWord.dim(0); row++){
					for(int col=0;col<UZWord.dim(1);col++){
						SaveFile <<(int)UZWord(row,col);
					}
					SaveFile <<endl;
				}
				SaveFile.close();*/
			
				//cout<<"VH :: ";
				// calculate vertical histogram
				for(int  i = 0;i<c2;i++){
					VH[i] = r2 - getSum('c',i,UZWord); /// VH: contains the vertical histogram result
					//cout<<VH[i]<<", ";
				}
				//cout<<endl;
				
				// reset count
				count = -1;
				
				startSearchIndex = 0; // Searching index for the vertical histogram
				// for the first non-zero column : special case when the first column
				// histogram value is non-zero
				//cout<<"CB :: ";
				if(VH[0]!=0){
					count = count + 1;
					CB[count] = -1; /// CB - Character Boundary
					//cout<<CB[count]<<", ";
					startSearchIndex = 1;
				}
				
				
				// find all the zero valued location from the histogram
				for(int i = startSearchIndex;i<c2;i++){
					if(VH[i]==0){
						count = count + 1;
						CB[count] = i; /// CB - Character Boundary
						//cout<<CB[count]<<", ";
					}
				}
				
				// for the last non-zero column : special case when the last column
				// histogram value is non-zero
				if(VH[c2-1]!=0){
					count = count + 1;
					CB[count] = c2; //% CB - Character Boundary
					//cout<<CB[count]<<", ";
				}
				//cout<<endl;
				// find out the boundary points
				thVal = 1;
				bool upperZonePresent;
				int topUnit, pixelCount;
				float thValTopUnit = 0.5;
				float ratioTopUnit;
				for(int i = 1;i<=count;i++){
					if(CB[i] - CB[i-1] > thVal){						 
						upperZonePresent = true;
						
						left_ = CB[i-1]+1;
						right_ = CB[i]-1;
						width_ = right_ - left_ + 1;
						
						// initialize topUnit
						topUnit = r2;
						
						for(int tpIndx=0; tpIndx<r2; tpIndx++){
							pixelCount = width_ - getSumLR(UZWord, tpIndx, left_, right_);
							if(pixelCount != 0){
								topUnit = tpIndx;
								break;
							}
						}

						height_ = ed_index_up_zone - topUnit + 1;
						
						//cout<<"topUnit :: "<<topUnit<<endl;
						//cout<<"r2 :: "<<r2<<endl;
						
						ratioTopUnit = (float)(topUnit+1)/r2;
						//cout<<"ratioTopUnit :: "<<ratioTopUnit<<endl;
						if(ratioTopUnit > thValTopUnit){
							//cout<<"here 1"<<endl;
							upperZonePresent = false;
						}
						
						// 2nd condition
						if(height_ < matraaLength | height_ < thHtUpUnit){
							//cout<<"here 2"<<endl;
							upperZonePresent = false;
						}
						                
						if(upperZonePresent){
							
							
							vector<int> tempUnit;
							
							tempUnit.push_back(topUnit);
							tempUnit.push_back(top + ed_index_up_zone);
							tempUnit.push_back(left_);
							tempUnit.push_back(right_);
							tempUnit.push_back(height_);
							tempUnit.push_back(width_);
							
							//% matraa start loc. Negative value means top upper zone
							//% units
							tempUnit.push_back(-1);
							
							//% matraa end loc
							tempUnit.push_back(-1);
							
							//% Zone Information
							tempUnit.push_back(1);
							
							Units.push_back(tempUnit);
							
							tempUnit.clear();
						}
					}
				}
			}
			
			// Set the totalNumberOfUnits
			totalNumberOfUnits = Units.size();
		}
		else{
			//cout<<"No Matraa"<<endl;
			// Neumerical word like 1954 will not have any matraa with them. So
			// they will fall in this category. In this case we will apply the
			// simple vertical histogram scanning here
			
			// Considering this unit don't have any matraa i.e. only middle zone
			// Condition : to overlook the aa-kar
			
			int VH[c1];
			
			if(r1 < 2*c1){
				// calculate vertical histogram
				for(int i = 0;i<c1;i++){
					VH[i] = r1 - getSum('c',i,TWord);//% VH: contains the vertical histogram result
				}
				
				//%% Extract the boundary points
				
				int count = -1;
				
				int startSearchIndex = 0; // Searching index for the vertical histogram
				
				int CB[c1]; // Character Boundary array
				
				// for the first non-zero column : special case when the first column
				// histogram value is non-zero
				if(VH[0]!=0){
					count = count + 1;
					CB[count] = -1; //% CB - Character Boundary
					startSearchIndex = 1;
				}
				
				// find all the zero valued location from the histogram
				for(int i = startSearchIndex;i<c1;i++){
					if(VH[i]==0){
						count = count + 1;
						CB[count] = i; //% CB - Character Boundary
					}
				}
				
				// for the last non-zero column : special case when the last column
				// histogram value is non-zero
				if(VH[c1-1]!=0){
					count = count + 1;
					CB[count] = c1; //% CB - Character Boundary
				}
				
				
				// find out the boundary points
				int thVal = 1;
				int count_ = -1;
				int left_, right_, width_;
				
				for(int i = 1; i<=count; i++){
					if(CB[i] - CB[i-1] > thVal){
						count_++;
						
						vector<int> tempUnit;
						
						left_ = CB[i-1] + 1;
						right_ = CB[i] - 1;
						width_ = right_ - left_ + 1;
						
						//% get the necessary info about the subimage or unit box
						
						//% top
						tempUnit.push_back(top);
						
						//% bottom
						for(int botIndx = r1-1; botIndx>=0;botIndx--){
							int tempWd = getSumLR(TWord, botIndx, left_, right_);
							
							if(tempWd != width_){
								tempUnit.push_back(bottom - (r1 - botIndx) + 1);
								break;
							}
						}
						
						// left
						tempUnit.push_back(left_);
						
						//% right
						tempUnit.push_back(right_);
						
						//% height
						tempUnit.push_back(tempUnit[1] - tempUnit[0] + 1);
						
						//% width
						tempUnit.push_back(width_);
						
						//% matraa start loc
						tempUnit.push_back(top); //% no matraa : just set the top
						
						//% matraa end loc
						tempUnit.push_back(top); //% no matraa : just set the top
						
						//% Zone Information
						tempUnit.push_back(2);
						
						Units.push_back(tempUnit);
						
						tempUnit.clear();
					}
				}
				
				// Set totalNumberOfUnits
				totalNumberOfUnits = count_;
			}
			else{
				vector<int> tempUnit;
				
				tempUnit.push_back(top);
				tempUnit.push_back(bottom);
				tempUnit.push_back(0);
				tempUnit.push_back(c-1);
				tempUnit.push_back(tempUnit[1] - tempUnit[0] + 1);
				tempUnit.push_back(tempUnit[3] - tempUnit[2] + 1);
				tempUnit.push_back(top); //% no matraa : just set the top
				tempUnit.push_back(top); //% no matraa : just set the top
				//% Zone Information
				tempUnit.push_back(2);
				
				Units.push_back(tempUnit);
				
				tempUnit.clear();
				
				// Set totalNumberOfUnits
				totalNumberOfUnits = 0;
			}
		}
		
		// build the units info
		UnitsInfo.resize(Units.size(), 9);
		
		for (int numUnit = 0; numUnit < Units.size(); numUnit++){
			//cout<<"< "<<numUnit<<" > -> [ "<<Units[numUnit][2]<<", "<<Units[numUnit][3]<<"]"<<endl;
			UnitsInfo(numUnit, 0) = Units[numUnit][0];
			UnitsInfo(numUnit, 1) = Units[numUnit][1];
			UnitsInfo(numUnit, 2) = Units[numUnit][2];
			UnitsInfo(numUnit, 3) = Units[numUnit][3];
			UnitsInfo(numUnit, 4) = Units[numUnit][4];
			UnitsInfo(numUnit, 5) = Units[numUnit][5];
			UnitsInfo(numUnit, 6) = Units[numUnit][6];
			UnitsInfo(numUnit, 7) = Units[numUnit][7];
			UnitsInfo(numUnit, 8) = Units[numUnit][8];
		}
	}
	
	void getMatraaLocPH(bytearray &TWord, int &ed_index_up_zone_ph, int &st_index_ml_zone_ph)
	{
		// get the size
		int r1=TWord.dim(0);
		int c1=TWord.dim(1);

	
		//cout<<"HH : ";
		// calculate horizontal histogram
		int HH[r1];
		for(int i=0;i<r1;i++){
			HH[i] = c1 - getSum('r', i, TWord);// HH: contains the horizontal histogram result
			//cout<<HH[i]<<", ";
		}
		//cout<<endl;
		
		int size = sizeof(HH)/sizeof(int);
		int inDx = getMaxIndx(HH, size);
		
		float matraaThVal = 0.7 * HH[inDx];
		bool stat = true;
		
		ed_index_up_zone_ph = inDx-1;
		st_index_ml_zone_ph = inDx+1;
		
		//cout<<"ed_index_up_zone :: "<<ed_index_up_zone<<", st_index_ml_zone :: "<<st_index_ml_zone<<endl;
		
		// get top most row of matraa
		stat = true;
		for(int tmr = inDx-1 ; tmr>=0; tmr--){
			if(stat && (HH[tmr] >= matraaThVal)){
				ed_index_up_zone_ph = tmr-1;
			}else{
				stat = false;
				break;
			}
		}
		
		// get bottom most row of matraa
		stat = true;
		for(int tmr=inDx+1; tmr<=ceil(r1/2); tmr++){
			if(stat && (HH[tmr] >= matraaThVal)){
				st_index_ml_zone_ph = tmr+1;
			}else{
				stat = false;
				break;
			}
		}
	}
	
	void getMatraaLoc(bytearray &TWord, int &ed_index_up_zone, int &st_index_ml_zone)
	{
		// get the size
		int r1=TWord.dim(0);
		int c1=TWord.dim(1);

		// << RLH Based matraa >>
				
		int RLH[r1];
		rlhist(TWord,RLH);
		
		int size = sizeof(RLH)/sizeof(int);
		int inDx=getMaxIndx(RLH, size);

		//cout<<"Matraa"<<endl;
		int stInDx=0;
		int enInDx=0;
		int count = 1;
		
		// fixing the start and end index (for standard deviation range)
		if(inDx-1<=0){
			stInDx = 0;
			enInDx = inDx+1;
		}else{
			stInDx = inDx-1;
			enInDx = inDx+1;
		}
		
		// find out the matraa and remove that
		float stdVal = getStdDevInRange(RLH,stInDx,enInDx);
		
		// fixing the start and end index (for matraa)
		if(inDx-2<=0){
			stInDx = 0;
			enInDx = inDx+2;
		}else{
			stInDx = inDx-2;
			enInDx = inDx+2;
		}
		
		count = 0;
		
		int peakWd[enInDx-stInDx+1];
		int peakInfo[enInDx-stInDx+1];
		
		for(int j=stInDx;j<=enInDx;j++){
			peakWd[count] = RLH[j];
			if(RLH[inDx] - RLH[j] <= stdVal*2){
				peakInfo[count] = 1;
			}else{
				peakInfo[count] = 0;
			}
			count = count + 1;
		}
		
		// set the upper and lower boundary of the matraa row
		// ed_index_up_zone : ending index of the upper zone
		int ed_index_up_zone_rlh;
		if(peakInfo[0]==1){
			ed_index_up_zone_rlh = stInDx - 1;
		}else if(peakInfo[1]==1){
			ed_index_up_zone_rlh = stInDx;
		}else{
			ed_index_up_zone_rlh = inDx - 1;
		}
		
		// st_index_ml_zone : starting index of the middle and lower zone
		int st_index_ml_zone_rlh;
		if(peakInfo[count-1]==1){
			st_index_ml_zone_rlh = enInDx + 1;
		}else if(peakInfo[count-2]==1){
			st_index_ml_zone_rlh = enInDx;
		}else{
			st_index_ml_zone_rlh = inDx + 1;
		}
		// << RLH Based matraa >>
		
		
		// << PH Based matraa >>
		
		//cout<<"HH : ";
		// calculate horizontal histogram
		int HH[r1];
		for(int i=0;i<r1;i++){
			HH[i] = c1 - getSum('r', i, TWord);// HH: contains the horizontal histogram result
			//cout<<HH[i]<<", ";
		}
		//cout<<endl;
		
		float matraaThVal = 0.7 * HH[inDx];
		int ed_index_up_zone_ph = inDx-1;
		int st_index_ml_zone_ph = inDx+1;
		bool stat = true;
		//cout<<"ed_index_up_zone :: "<<ed_index_up_zone<<", st_index_ml_zone :: "<<st_index_ml_zone<<endl;
		// get top most row of matraa
		stat = true;
		for(int tmr = inDx-1 ; tmr>=0; tmr--){
			if(stat && (HH[tmr] >= matraaThVal)){
				ed_index_up_zone_ph = tmr-1;
			}else{
				stat = false;
				break;
			}
		}
		
		// get bottom most row of matraa
		stat = true;
		for(int tmr=inDx+1; tmr<=ceil(r1/2); tmr++){
			if(stat && (HH[tmr] >= matraaThVal)){
				st_index_ml_zone_ph = tmr+1;
			}else{
				stat = false;
				break;
			}
		}
		
		// Selection of matraa approach
		int choice = 3;
		
		if(choice == 1){
			//////// <<<< 1 -- Minimum/Maximum of RLH and PH based method 
			// take the minimum for matraa_start_loc
			if(ed_index_up_zone_ph < ed_index_up_zone_rlh)
				ed_index_up_zone = ed_index_up_zone_ph;
			else
				ed_index_up_zone = ed_index_up_zone_rlh;
			
			// take the maximum for matraa_end_loc
			if(st_index_ml_zone_ph > st_index_ml_zone_rlh)
				st_index_ml_zone = st_index_ml_zone_ph;
			else
				st_index_ml_zone = st_index_ml_zone_rlh;
		}else if(choice == 2){
			//////// <<<< 2 -- only PH based method
			ed_index_up_zone = ed_index_up_zone_ph;
			st_index_ml_zone = st_index_ml_zone_ph;
		}else{
			//////// <<<< 3 -- only RLH based method
			ed_index_up_zone = ed_index_up_zone_rlh;
			st_index_ml_zone = st_index_ml_zone_rlh;
		}
	}
	
	void getWords(bytearray &I, intarray &WordsInfo){
		
		// <print the image>
		/*ofstream SaveFile("matlab_image.txt");
		for(int row=0; row<I.dim(0); row++){
		for(int col=0;col<I.dim(1);col++){
		SaveFile <<(int)I(row,col)<<" ";
		}
		SaveFile <<endl;
		}
		SaveFile.close();*/
		
		//size of the line image
		int r=I.dim(0);
		int c=I.dim(1);
		
		//Reference peak of the line
		//int RLHist[r];
		//rlhist(I,RLHist);
		//int maxVal=getMaxVal(RLHist);
		//int refPeak=getMaxIndx(RLHist);
		
		//Find the vertical histogram of the line
		int Vhist[c];
		for(int i=0;i<c;i++){
			Vhist[i] = r - getSum('c',i, I); //Vhist: contains the vertical histogram result
		}
		
		// Extract the words
		int count = -1;
		int startIndex = 0;
		
		// special condition : for the first line
		// check whether the first row is counting or not
		int Minima[c];
		if(Vhist[0] != 0){
			count = count + 1;
			Minima[count] = -1;
			startIndex = 1;
		}
		
		//Locate all the minima of the histogram
		int thVal = 1; // minimum value to be minima
		for(int i=startIndex; i<c; i++){
			if(Vhist[i] < thVal){
				count = count + 1;
				Minima[count] = i;
			}
		}
		// special condition : for the last word
		// check whether the last row is counting or not
		if(Vhist[c-1] != 0){
			count = count + 1;
			Minima[count] = c;
			
		}
		
		//Extract the actual minima (pruning the redundand minimas)
		// Pruning with one more conditions: If the line / block width is less than
		// a threshold value (minimam gap) then we can eliminate that
		
		thVal = 1; // threshold value to determine the condition to be a line / block
		int count_ = -1;
		int Gaps[count][2];
		for (int i=1;i<=count;i++){
			if(Minima[i] - Minima[i-1] > thVal){
				count_ = count_ + 1;
				Gaps[count_][0] = Minima[i-1] + 1;
				Gaps[count_][1] = Minima[i] - 1;
			}
		}
		
		// correct the word boundary
		
		// Get the spaces between primary segmented words
		int Gapw[count_];
		
		for(count = 0;count<count_;count++){
			Gapw[count] = Gaps[count+1][0] - Gaps[count][1];
		}
		
		if(count_ > 0){
			//%%%% #################     Special Module    ################# %%%%%% 
			//To handle large gap that may effect the proper word segmentation
			
			int size = sizeof(Gapw)/sizeof(int);
			
			// mean or average
			float avgGap=getMean(Gapw, size);
			// median
			int medianGap = getMedian(Gapw, size);
			// standard deviation
			float stdGap = getStdDev(Gapw, size);
			
			// Condition : Determine the presence of large gap
			int largeGapPresent;	
			if ( stdGap - avgGap   > medianGap){
				largeGapPresent = 1;
			}else{
				largeGapPresent = 0;
			}
			
			// Normalize the gaps
			while(largeGapPresent == 1){
				//int maxVal=getMaxVal(Gapw);
				int maxValLoc=getMaxIndx(Gapw, size);
				Gapw[maxValLoc] = medianGap;
				
				// mean or average
				avgGap = getMean(Gapw, size);//what to do
				// median
				medianGap = getMedian(Gapw, size);//what to do
				// standard deviation
				stdGap = getStdDev(Gapw, size);//what to do
				
				// Condition : Determine the presence of large gap
				if ( avgGap - stdGap  > medianGap){
					largeGapPresent = 1;
				}else{
					largeGapPresent = 0;
				}
			}
			
			// Determine the gap factor which will determine the gap between words and
			// units
			float gapFactor=0;
			if(medianGap < stdGap){
				gapFactor = avgGap;
			}
			else{
				gapFactor = stdGap;
			}
			
						
			// Actual gap analysis
			int GapLoc[count_];
			for(count = 0 ; count< count_; count++){
				if(Gapw[count] - gapFactor > 0){
					GapLoc[count] = 1;
				}else{
					GapLoc[count] = 0;
				}
			}
			
			// Finalizing the word boundaries
			int numOfWord = 0;
			int Words [count_ - 1][2];
			int wordStart = Gaps[0][0];//what to do
			int wordEnd   = Gaps[0][1];//what to do
			
			for(count = 0 ;count<count_;count++){
				if(GapLoc[count] == 0){
					wordEnd = Gaps[count+1][1];//what to do
				}else{
					Words[numOfWord][0] = wordStart;
					Words[numOfWord][1] = wordEnd;
					
					wordStart = Gaps[count+1][0];
					wordEnd   = Gaps[count+1][1];
					
					numOfWord = numOfWord + 1;
				}
			}
			
			// for the last word boundary
			Words[numOfWord][0] = wordStart;
			Words[numOfWord][1] = wordEnd;
			
			// build the wordsn info
			WordsInfo.resize(numOfWord+1, 2);
			for (int numW = 0; numW <= numOfWord; numW++){
				//cout<<"< "<<numW<<" > -> [ "<<Words[numW][0]<<", "<<Words[numW][1]<<"]"<<endl;
				WordsInfo(numW, 0) = Words[numW][0];
				WordsInfo(numW, 1) = Words[numW][1];
			}
		}else{
			WordsInfo.resize(1, 2);
			WordsInfo(0, 0) = Gaps[0][0];
			WordsInfo(0, 1) = Gaps[0][1];
		}
	}
	
	void getWordsPHBased(bytearray &I, intarray &WordsInfo){
		
		// <print the image>
		/*ofstream SaveFile("matlab_image.txt");
		for(int row=0; row<I.dim(0); row++){
		for(int col=0;col<I.dim(1);col++){
		SaveFile <<(int)I(row,col)<<" ";
		}
		SaveFile <<endl;
		}
		SaveFile.close();*/
		
		//size of the line image
		int r=I.dim(0);
		int c=I.dim(1);
		
		//Reference peak of the line
		//int RLHist[r];
		//rlhist(I,RLHist);
		//int maxVal=getMaxVal(RLHist);
		//int refPeak=getMaxIndx(RLHist);
		
		//Find the vertical histogram of the line
		int Vhist[c];
		for(int i=0;i<c;i++){
			Vhist[i] = r - getSum('c',i, I); //Vhist: contains the vertical histogram result
		}
		
		// Extract the words
		int count = -1;
		int startIndex = 0;
		
		// special condition : for the first line
		// check whether the first row is counting or not
		int Minima[c];
		if(Vhist[0] != 0){
			count = count + 1;
			Minima[count] = -1;
			startIndex = 1;
		}
		
		//Locate all the minima of the histogram
		int thVal = 1; // minimum value to be minima
		for(int i=startIndex; i<c; i++){
			if(Vhist[i] < thVal){
				count = count + 1;
				Minima[count] = i;
			}
		}
		// special condition : for the last word
		// check whether the last row is counting or not
		if(Vhist[c-1] != 0){
			count = count + 1;
			Minima[count] = c;
			
		}
		
		//Extract the actual minima (pruning the redundand minimas)
		// Pruning with one more conditions: If the line / block width is less than
		// a threshold value (minimam gap) then we can eliminate that
		
		thVal = 1; // threshold value to determine the condition to be a line / block
		int count_ = -1;
		int Gaps[count][2];
		for (int i=1;i<=count;i++){
			if(Minima[i] - Minima[i-1] > thVal){
				count_++;
				Gaps[count_][0] = Minima[i-1] + 1;
				Gaps[count_][1] = Minima[i] - 1;
			}
		}
		
		
		WordsInfo.resize(count_+1, 2);
		for (int i=0;i<=count_;i++){
			WordsInfo(i, 0) = Gaps[i][0];
			WordsInfo(i, 1) = Gaps[i][1];
		}
	}
	
	void getLines(bytearray &I, intarray &Lines){
		
		// <print the image>
		/*ofstream SaveFile("I.txt");
		for(int row=0; row<I.dim(0); row++){
		for(int col=0;col<I.dim(1);col++){
		SaveFile <<(int)I(row,col)<<" ";
		}
		SaveFile <<endl;
		}
		SaveFile.close();*/
		
		//size of the line image
		int r=I.dim(0);
		int c=I.dim(1);
		
		//// Step 1
		// Find the run length histogram of the document
		int RLHist[r];
		rlhist(I,RLHist);

		//// Step 2
		int thValLineHt = 10; // Threshold for the minimum height of a line
		int thValMinima = 1; // Threshold for the minimum value to be minima
		int count = -1;
		int Minima[c];
		
		// special condition : for the first line
		// check whether the first row is counting or not
		if(RLHist[0] != 0){
			count++;
			Minima[count] = 0;
		}
		
		// Locate all the minima of the histogram
		// check with a threshold value based on the minimum value of the minima
		for (int i = 1 ; i<r; i++){
			if(RLHist[i] <= thValMinima){
				count++;
				Minima[count] = i;
			}
		}
		
		// special condition : for the last line
		// check with a threshold value based on the probable height of a line
		if(r-Minima[count] > thValLineHt){
			count++;
			Minima[count] = r-1;
		}
		
		// Extract the actual minima (pruning the redundand minimas)
		// Pruning with one more conditions: If the line / block width is less than
		// a threshold value then we can eliminate that line
		int Minima_exact[count+1][2];
		int count_ = -1;
		
		for (int i = 1; i<=count; i++){
			if(Minima[i] - Minima[i-1] > 1){
				count_++;
				Minima_exact[count_][0] = Minima[i-1];
				Minima_exact[count_][1] = Minima[i];
			}
		}
		
		//// ##### Module : Select and elminate the Over-segmentation / Splitting
		//// errors in the lines
		
		// Calculate the height of the prilimanary identified lines
		int PLineHeight[count_+1];
		count = -1;
		
		for (int i=0; i<=count_; i++){
			count++;
			PLineHeight[count] = Minima_exact[count][1] - Minima_exact[count][0] + 1;
		}
		
		// calculate the average, median and standard deviation of the prilimanary
		// identified line height
		
		int size = sizeof(PLineHeight)/sizeof(int);
		
		// mean or average
		float avgHt = getMean(PLineHeight, size);
		// median
		float medianHt = getMedian(PLineHeight, size);
		// standard deviation
		float stdHt = getStdDev(PLineHeight, size);
		
		// calculate the validity of the line height
		int vCount = -1;
		int Validity[count+1];
		
		for (int i= 0; i<=count; i++){
			vCount++;
			if(PLineHeight[i] < avgHt - stdHt ){
				Validity[vCount] = 0;
			}else{
				Validity[vCount] = 1;
			}
		}
		
		//// Data Structure : LineInfo
		// Holds the information of each line after solving over segmentation
		// problem. Structure is as follows:
		// 1. Line start Index
		// 2. Line end Index
		// 3. Status & Index of undersegmentated units (0 - no under segmentation)
		// and (index of undersegmented line - if there is undersegmentation)
		
		// Determine the Lines
		int stOfLine = 0;
		int endOfLine = 0;
		int LineInfo[count+1][3];
		count_ = -1;
		int ii = 0;
		
		while(ii < count){
			// Check the condition for the possibility of merging two lines
			if(( Validity[ii]==0 || Validity[ii+1]==0 ) && ( Minima_exact[ii][1] - Minima_exact[ii+1][1] == 0)){
				stOfLine = Minima_exact[ii][0];
				endOfLine = Minima_exact[ii+1][1];
				
				count_++;
				
				LineInfo[count_][0] = stOfLine;
				LineInfo[count_][1] = endOfLine;
				
				ii = ii + 2;
			}else{
				stOfLine = Minima_exact[ii][0];
				endOfLine = Minima_exact[ii][1];
				
				count_++;
				
				LineInfo[count_][0] = stOfLine;
				LineInfo[count_][1] = endOfLine;
				
				ii = ii + 1;
			}
		}
		
		// for the last line
		if(ii == count){
			stOfLine = Minima_exact[ii][0];
			endOfLine = Minima_exact[ii][1];
			
			count_++;
			LineInfo[count_][0] = stOfLine;
			LineInfo[count_][1] = endOfLine;
		}
		
		
		//// ##### Module : Select and elminate the Undersegmentation / Joinning
		//// errors in the lines
		
		// Calculate the height of the secondary identified lines
		int SLineHeight[count_+1];
		count = -1;
		
		for(int i = 0; i<=count_; i++){
			count++;
			SLineHeight[count] = LineInfo[count][1] - LineInfo[count][0] + 1;
		}
		
		// calculate the average, median and standard deviation of the secondary
		// identified line height
		size = sizeof(SLineHeight)/sizeof(int);
		
		avgHt = getMean(SLineHeight, size);
		medianHt = getMedian(SLineHeight, size);
		stdHt = getStdDev(SLineHeight, size);
		
		//// Data Structure for UnderSegmentedLinesInfo
		//// 1. Line Index
		//// 2. Position of segmentation
		
		// Find out the under segmentation status and save necessary statistics
		int numOfUndSegLines = -1; // count number of under segmented lines
		int ratio = 0;
		int locSeg = 0;
		int startPos = 0;
		int endPos = 0;
		int peak1 = 0;
		int peak2 = 0;
		int peak1Loc = 0;
		int peak2Loc = 0;
		int valLoc = 0;
		int valleyLoc = 0;
		int UnderSegmentedLinesInfo[count_+1][2];

		for(int i = 0; i<= count_; i++){
			if(SLineHeight[i] > (avgHt + 2 * stdHt)){
				// Get the ratio of Line Height VS Average Height
				ratio = (float)SLineHeight[i] / avgHt;
				
				// get the probable location for segmentation
				locSeg = ceil(LineInfo[i][0] + (float)(LineInfo[i][1] - LineInfo[i][0])/ratio);
				
				// Get start and end position
				startPos = LineInfo[i][0];
				endPos = LineInfo[i][1];
				
				// get the highest peak of the first part
				peak1 = getMaxIndxInRange(RLHist, startPos, locSeg);
				
				// Actual Peak 1 location
				peak1Loc = startPos + peak1 - 1;
				
				// get the highest peak of the second part
				peak2 = getMaxIndxInRange(RLHist, locSeg, endPos);
				
				// Actual Peak 2 location
				peak2Loc = locSeg + peak2 - 1;
				
				// get the valley among the two peak , which is the location of the
				// segmentation
				valLoc = getMaxIndxInRange(RLHist, peak1Loc, peak2Loc);
				
				// Actual Valley location
				valleyLoc = peak1Loc + valLoc - 1;
				
				// Increment numOfUndSegLines
				numOfUndSegLines++;
				
				// Save the index of numOfUndSegLines as an indication of the
				// presence of under segmented line.
				LineInfo[i][2] = numOfUndSegLines;
				
				// Index of the Line
				UnderSegmentedLinesInfo[numOfUndSegLines][0] = i;
				
				// Position of segmentation
				UnderSegmentedLinesInfo[numOfUndSegLines][1] = valleyLoc;
			}else{
				// Just assign value - 0 as an indication of no under segmentation
				LineInfo[i][2] = 0;
			}
		}
		
		////////////////////////////////////////////////
		////// Now merge the LineInfo with UnderSegmentedLines info
		int numOfLine = 0;
		int countUndSegLine = -1;
		int lineIndex;
		int indSeg = 0;
		//Lines.resize(count_+1, 2);
		
		
		if(numOfUndSegLines > 0){
			Lines.resize(count_+1+numOfUndSegLines, 2);
			
			// Get the number of line
			numOfLine = count_;
			
			// Under Segmented Lines Count
			countUndSegLine = -1;
			
			// Set the new line index
			lineIndex = -1;
			for(int i = 0; i<=numOfLine; i++){
				if(LineInfo[i][2] != 0){
					countUndSegLine++;

					// Get the index of segmentation
					indSeg = UnderSegmentedLinesInfo[countUndSegLine][1];

					//// Set the first line
					lineIndex++;
					// Start Index
					Lines(lineIndex, 0) = LineInfo[i][0];
					// End Index
					Lines(lineIndex, 1) = indSeg;
					
					//// Set the Second line
					
					lineIndex++;
					// Start Index
					Lines(lineIndex, 0) = indSeg;
					// End Index
					Lines(lineIndex, 1) = LineInfo[i][1];
				}else{
					lineIndex++;
					// Start Index
					Lines(lineIndex, 0) = LineInfo[i][0];
					
					// End Index
					Lines(lineIndex, 1) = LineInfo[i][1];
					
				}
			}
		}else{
			Lines.resize(count_+1, 2);
			for(int i=0; i<=count_; i++){
				Lines(i, 0) = LineInfo[i][0];
				Lines(i, 1) = LineInfo[i][1];
			}
		}
		////////////////////////////////////////////////
	}
	
	
	void getLinesPHBased(bytearray &I, intarray &Lines){
	//size of the line image
		int r=I.dim(0);
		int c=I.dim(1);
		
		//// Step 1
		// Find the run length histogram of the document
		int RLHist[r];
		rlhist(I,RLHist);

		//// Step 2
		int thValLineHt = 10; // Threshold for the minimum height of a line
		int thValMinima = 1; // Threshold for the minimum value to be minima
		int count = -1;
		int Minima[c];
		
		// special condition : for the first line
		// check whether the first row is counting or not
		if(RLHist[0] != 0){
			count++;
			Minima[count] = 0;
		}
		
		// Locate all the minima of the histogram
		// check with a threshold value based on the minimum value of the minima
		for (int i = 1 ; i<r; i++){
			if(RLHist[i] <= thValMinima){
				count++;
				Minima[count] = i;
			}
		}
		
		// special condition : for the last line
		// check with a threshold value based on the probable height of a line
		if(r-Minima[count] > thValLineHt){
			count++;
			Minima[count] = r-1;
		}
		
		// Extract the actual minima (pruning the redundand minimas)
		// Pruning with one more conditions: If the line / block width is less than
		// a threshold value then we can eliminate that line
		int Minima_exact[count+1][2];
		int count_ = -1;
		
		for (int i = 1; i<=count; i++){
			if(Minima[i] - Minima[i-1] > 1){
				count_++;
				Minima_exact[count_][0] = Minima[i-1];
				Minima_exact[count_][1] = Minima[i];
			}
		}
		
		Lines.resize(count_+1, 2);
		
		
		for (int i = 0; i<=count_; i++){
			Lines(i, 0) = Minima_exact[i][0];
			Lines(i, 1) = Minima_exact[i][1];
		}
	}
	
	int getBoxes(bytearray &I, intarray &Boxes){
		
		bytearray matlab_page_image;
		bytearray temp_image;
		
		temp_image.resize(I.dim(0), I.dim(1));
		
		for(int x=0; x<temp_image.dim(0); x++){
			for(int y=0; y<temp_image.dim(1); y++){
				temp_image(x, y) = I(x,y);
			}
		}
		
		
		// make the image proper binary
		for(int x=0; x<temp_image.dim(0); x++){
			for(int y=0; y<temp_image.dim(1); y++){
				if(temp_image(x,y)==255) {
					temp_image(x,y) = 0;
				}else{
					temp_image(x,y) = 1;
				}
			}
		}
		
		// Build Matlab format Image
		matlab_page_image.resize(temp_image.dim(1), temp_image.dim(0));
		
		int xx = matlab_page_image.dim(0) - 1;
		
		for(int x=0; x<matlab_page_image.dim(0); x++){
			for(int y=0; y<matlab_page_image.dim(1); y++){
				matlab_page_image(x, y) = temp_image(y, (xx-x)); 
			}
		}
		
		//size of the line image
		int r = matlab_page_image.dim(0);
		
		//// Extract the text lines and relevant information
		intarray Lines;
		
		getLinesPHBased(matlab_page_image, Lines);
				
		//// for each Text Line do further processing
		int numOfline = Lines.dim(0);
		
		//cout<<"Number of lines in the image: "<<numOfline<<endl;
		
		int count = -1;
		Boxes.resize(numOfline*100, 4);
		
		for(int i = 0; i<numOfline; i++){
			bytearray Line;
			cropingImage(matlab_page_image, Line, Lines(i, 0), Lines(i, 1), 0, matlab_page_image.dim(1)-1);
			
			// extract the words from the line
			intarray Words;
			getWordsPHBased(Line, Words);
			//getWords(Line, Words);
			
			///// for each extracted words
			int numOfword = Words.dim(0);
			
			cout<<"Number of words in the line [ "<<i<<" ]: "<<numOfword<<endl;
			
			for(int ind = 0; ind<numOfword; ind++){
				// get the word in a seperate matrix
				bytearray Word;
				cropingImage(Line, Word, 0, Line.dim(0)-1, Words(ind, 0), Words(ind, 1));
				
				// extract the proper boundary (top and bottom)
				int rw = Word.dim(0);
				int cw = Word.dim(1);
				
				// get the top
				int top = 0;
				for(int ii=0; ii<rw; ii++){
					int tempWd = getSumLR(Word, ii, 0, cw-1);
					
					if(cw - tempWd != 0){
						top = ii;
						break;
					}
				}
				
				top = Lines(i, 0) + top;
				// get the bottom
				int bottom = rw-1;
				for(int ii = rw-1 ; ii>=0; ii--){
					int tempWd = getSumLR(Word, ii, 0, cw-1);
					
					if(cw - tempWd != 0){
						bottom = ii;
						break;
					}
				}
				
				bottom = Lines(i, 0) + bottom;
				
				count = count + 1;
				
				Boxes(count, 0) = Words(ind, 0);
				Boxes(count, 1) = r - bottom;
				Boxes(count, 2) = Words(ind, 1);
				Boxes(count, 3) = r - top;
			}
		}
		
		return count;
	}
	
	// function for calculating the crossing information of an image
	void getCrossing(colib::bytearray &I, int* CR){
		//%% get the size of I
		
		int rNum=I.dim(0);
		int cNum=I.dim(1);
		
		for(int col = 0;col<cNum;col++){
			int crCount = 0;// %% crossing count
			
			if(I(0, col) == 0){
				crCount++;
			}
			
			for(int row = 0; row<rNum-1; row++){
				//%% condition : present row is white and next row is black
				if(I(row, col)==1 && I(row+1, col)==0){
					crCount++;
				}
			}
			CR[col] = crCount;
		}
	}
	
	// function for obtaining vertical bar location from an image
	int getVerticalBar(colib::bytearray &I){
		// initialize vBarLoc with -1
		int vBarLoc = -1;
		
		// get the size of the image
		int r=I.dim(0);
		int c=I.dim(1);
		
		// get the vertical histogram of the image
		int VH[c];
		for(int i = 0;i<c;i++){
			VH[i] = r - getSum('c',i,I);// VH: contains the vertical histogram result
		}
		//cout<<"VH: contains the vertical histogram result"<<endl;
		// get the value and location of the maximum histogram
		int val,loc;
		val= getMaxVal(VH, c);
		loc= getMaxIndx(VH, c);
		
		// The position of the vertical bar is the left most column where the
		// number of black pixel is 80% or more of the character height.
		if(val >= 0.8 * (float)r){
			vBarLoc = loc;
		}
		
		return vBarLoc;
	}
	
	void prepareColorImageForLineImageTraining(colib::intarray &color_segmentation, colib::bytearray &orig_image){
		bytearray Timage;
		copy(Timage, orig_image);
		
		// make the image proper binary
		make_page_binary_and_black(Timage);
		for(int row=0; row<Timage.dim(0); row++){
			for(int col=0;col<Timage.dim(1);col++){
				if(Timage(row, col)!=0)
					Timage(row, col) = 0;
				else
					Timage(row, col) = 1;
			}
		}
		
		// Build Matlab format Image
		bytearray matlab_page_image;
		int col=orig_image.dim(0);
		int row=orig_image.dim(1);
		
		matlab_page_image.resize(row, col);
		
		int xx = matlab_page_image.dim(0) - 1;
		for(int x=0; x < matlab_page_image.dim(0); x++){
			for(int y=0; y<matlab_page_image.dim(1); y++){
				matlab_page_image(x, y) = Timage(y, (xx-x)); 
			}
		}
		
		// Get the word boundaries
		intarray word_info;
		getWords(matlab_page_image, word_info);
		
		int numOfUnits = -1;
		
		vector<UnitInformation> LineWordsInfo;
		
		for(int x=0; x<word_info.dim(0); x++){
			intarray units_info;
			
			getUnits(matlab_page_image, units_info, word_info(x, 0), word_info(x, 1));
			
			int numOfUnits = units_info.dim(0);
			int numOfElementsPerUnit = units_info.dim(1);
			//cout<<"numOfUnits ::: "<<numOfUnits<<endl;
			// cout<<x<<" -> NOU :: "<<numOfUnits<<", numOfElements :: "<<numOfElementsPerUnit<<endl;
			
			// sort the units info by left boundary information
			int** sortedUnitsInfo = allocate2DArray(numOfUnits, numOfElementsPerUnit);
			sortByRows(units_info, sortedUnitsInfo, numOfUnits, numOfElementsPerUnit, 2);
			
			// individual units
			int yy = word_info(x, 0);
			for(int y=0; y<units_info.dim(0); y++){
				UnitInformation uinfo;				
				
				// set the actual height
				uinfo.height = sortedUnitsInfo[y][4];
				
				// set the actual width
				uinfo.width = sortedUnitsInfo[y][5];
				
				// set top info
				uinfo.top = sortedUnitsInfo[y][0];
				
				// set bottom info
				uinfo.bottom = sortedUnitsInfo[y][1];
				
				//cout<<"Top ::: "<<uinfo.top<<" Bottom ::: "<<uinfo.bottom<<endl;
				
				// set left info
				uinfo.left = yy + sortedUnitsInfo[y][2];
				
				// set right info
				uinfo.right = yy + sortedUnitsInfo[y][3];
				
				// set the matraa starting position
				uinfo.matraaSrartLoc = sortedUnitsInfo[y][6];
				
				// set the matraa ending positionrig
				uinfo.matraaEndLoc = sortedUnitsInfo[y][7];
				
				// set the line number
				uinfo.lineNumber = 1;
				
				// set the word number
				uinfo.wordNumber = x;
				
				// set the unit number
				uinfo.unitNumber = y;
				
				// set the zone info
				uinfo.zoneNumber = sortedUnitsInfo[y][8];
				
				// set the shadow units location as -1
				uinfo.locOfShadowUnit = -1;
				
				LineWordsInfo.push_back(uinfo);
			}
			
			// free memory
			delete2DArray(sortedUnitsInfo, numOfUnits);
		}
		
		numOfUnits = LineWordsInfo.size();
		
		bytearray segmentation;
		intarray csegmentation;
		
		segmentation.resize(matlab_page_image.dim(0), matlab_page_image.dim(1));
		csegmentation.resize(matlab_page_image.dim(0), matlab_page_image.dim(1));
		
		for(int i=0;i<matlab_page_image.dim(0);i++){
			for(int j=0;j<matlab_page_image.dim(1);j++){
				segmentation(i,j)  = 1;
				csegmentation(i,j) = 16777215;
			}
		}
		
		int top=0;
		int bottom=0;
		int left_=0;
		int right_=0;
		
		for(int index = 0; index<numOfUnits; index++){
			top    = LineWordsInfo[index].top;
			bottom = LineWordsInfo[index].bottom;
			left_  = LineWordsInfo[index].left;
			right_ = LineWordsInfo[index].right;
			
			for(int i=top;i<=bottom;i++){
				for(int j=left_;j<=right_;j++){
					if(matlab_page_image(i,j)==0){
						segmentation(i,j)=0;
					}
				}
			}
		}
		
		/*SaveFile.open("segmentation.txt");
		for(int row=0; row<segmentation.dim(0); row++){
		for(int col=0;col<segmentation.dim(1);col++){
		SaveFile <<(int)segmentation(row,col)<<" ";
		}
		SaveFile <<endl;
		}
		SaveFile.close();*/
		
		// << First level filtering is done. Now extract units and color >>
		// << segmentation array contains image info of a line >>
		
		// extract the words from the line
		int r = segmentation.dim(0);
		
		intarray Words;
		getWords(segmentation, Words);
		
		///// for each extracted words
		int numOfword = Words.dim(0);
		
		//cout<<"Number of words in the line [ "<<i<<" ]: "<<numOfword<<endl;
		
		for(int ind = 0; ind<numOfword; ind++){
			// get the word in a seperate matrix
			bytearray Word;
			cropingImage(segmentation, Word, 0, r-1, Words(ind, 0), Words(ind, 1));
			
			// extract the proper boundary (top and bottom)
			int rw = Word.dim(0);
			int cw = Word.dim(1);
			
			// get the top
			int top = 0;
			for(int ii=0; ii<rw; ii++){
				int tempWd = getSumLR(Word, ii, 0, cw-1);
				
				if(cw - tempWd != 0){
					top = ii;
					break;
				}
			}
			
			// get the bottom
			int bottom = rw-1;
			for(int ii = rw-1 ; ii>=0; ii--){
				int tempWd = getSumLR(Word, ii, 0, cw-1);
				
				if(cw - tempWd != 0){
					bottom = ii;
					break;
				}
			}
			
			left_   = Words(ind, 0);
			right_  = Words(ind, 1);
			
			for(int i=top;i<=bottom;i++){
				for(int j=left_;j<=right_;j++){
					if(segmentation(i,j)==0){
						csegmentation(i,j)=ind+1;
					}
				}
			}
			
		}
		
		// Return back to OCROPUS image format
		color_segmentation.resize(segmentation.dim(1), segmentation.dim(0));
		
		int yy = color_segmentation.dim(1) - 1;
		
		for(int x=0; x<color_segmentation.dim(0); x++){
			for(int y=0; y<color_segmentation.dim(1); y++) {
				color_segmentation(x, y) = csegmentation((yy-y), x); 
			}
		}
	}
	
	void prepareColorImageFromWordBoundary(colib::intarray &color_segmentation, colib::bytearray &orig_image){
		bytearray Timage;
		copy(Timage, orig_image);
		
		// make the image proper binary
		make_page_binary_and_black(Timage);
		for(int row=0; row<Timage.dim(0); row++){
			for(int col=0;col<Timage.dim(1);col++){
				if(Timage(row, col)!=0)
					Timage(row, col) = 0;
				else
					Timage(row, col) = 1;
			}
		}
		
		// Build Matlab format Image
		bytearray matlab_page_image;
		int col=orig_image.dim(0);
		int row=orig_image.dim(1);
		
		matlab_page_image.resize(row, col);
		
		int xx = matlab_page_image.dim(0) - 1;
		for(int x=0; x < matlab_page_image.dim(0); x++){
			for(int y=0; y<matlab_page_image.dim(1); y++){
				matlab_page_image(x, y) = Timage(y, (xx-x)); 
			}
		}
		
		// Get the word boundaries
		intarray word_info;
		getWords(matlab_page_image, word_info);
		
		int numOfUnits = word_info.dim(0);
		
		intarray segmentation;
		
		segmentation.resize(matlab_page_image.dim(0), matlab_page_image.dim(1));
		
		for(int i=0;i<matlab_page_image.dim(0);i++){
			for(int j=0;j<matlab_page_image.dim(1);j++){
				segmentation(i,j) = 16777215;
			}
		}
		
		int top=0;
		int bottom=0;
		int left_=0;
		int right_=0;
		
		for(int index = 0; index<numOfUnits; index++){
			top = 0;
			bottom = row-1;
			left_  = word_info(index, 0);
			right_ = word_info(index, 1);
			
			for(int i=top;i<=bottom;i++){
				for(int j=left_;j<=right_;j++){
					if(matlab_page_image(i,j)==0){
						segmentation(i,j) = index+1;
					}
				}
			}
		}
		
		// Return back to OCROPUS image format
		color_segmentation.resize(segmentation.dim(1), segmentation.dim(0));
		
		int yy = color_segmentation.dim(1) - 1;
		
		for(int x=0; x<color_segmentation.dim(0); x++){
			for(int y=0; y<color_segmentation.dim(1); y++) {
				color_segmentation(x, y) = segmentation((yy-y), x); 
			}
		}
	}
	
	void rlhist(bytearray &I, int* RLHist){
		//size of the line image
		int r = I.dim(0);
		int c = I.dim(1);
		
		
		int **J = allocate2DArray(r, c);
		
		/// inverse of I in J
		for(int i=0;i<r;i++){
			for(int j=0;j<c;j++){
				if(I(i,j)==0)
					J[i][j]=1;
				else
					J[i][j]=0;
			}
		}
		
		/// defining and initialize
		int **K = allocate2DArray(r, c);	
		
		for(int i=0;i<r;i++){
			for(int j=0;j<c;j++){
				K[i][j]=0;
			}
		}
		
		// initialize the first column of K
		for(int i=0;i<r;i++)
			K[i][0]=J[i][0];
		
		
		for(int i = 0;i<r;i++){
			for(int j = 1 ;j<c;j++){
				if (J[i][j] == 1)
					K[i][j] = K[i][j-1] + J[i][j];
				else
					K[i][j] = 0;
			}
			
			// Get the total run lenth value
			// Initialize value
			RLHist[i] = 0;
			for(int j = 0 ;j<c;j++){
				RLHist[i] += K[i][j];
			}
		}
		
		// delete J and K arrays
		delete2DArray(J, r);
		delete2DArray(K, r);
	}
	
	int getMaxVal(int a[], int limit){
		int maxVal=a[0];
		for(int i=1;i<limit;i++){
			if(a[i]>maxVal){
				maxVal=a[i];
			}
		}
		return maxVal;
	}
	
	int getMaxIndx(int a[], int limit){
		int maxVal=a[0];
		int indx=0;
		for(int i=1;i<limit;i++){
			if(a[i]>maxVal){
				maxVal=a[i];
				indx=i;
			}
		}
		return indx;
	}
	
	int getMaxIndxInRange(int a[],int start,int end){
		int N = end-start+1; 
		int aa[N];
		
		for (int i=start, ii=0 ; i<=end; i++, ii++){
			aa[ii] = a[i];
		}
		
		int maxVal=aa[0];
		int indx=0;
		
		for(int i=1;i<N;i++){
			if(aa[i]>maxVal){
				maxVal=aa[i];
				indx=i;
			}
		}
		return indx;
	}
	
	int getMinVal(int a[], int limit){
		int minVal=a[0];
		for(int i=1;i<limit;i++){
			if(a[i]<minVal){
				minVal=a[i];
			}
		}
		return minVal;
	}
	
	int getMinIndx(int a[], int limit){
		int minVal=a[0];
		int indx=0;
		for(int i=1;i<limit;i++){
			if(a[i]<minVal){
				minVal=a[i];
				indx=i;
			}
		}
		return indx;
	}
	
	int getMinIndxInRange(int a[],int start,int end){
		int N = end-start+1; 
		int aa[N];
		
		for (int i=start, ii=0 ; i<=end; i++, ii++){
			aa[ii] = a[i];
		}
		
		int minVal=aa[0];
		int indx=0;
		
		for(int i=1;i<N;i++){
			if(aa[i]<minVal){
				minVal=aa[i];
				indx=i;
			}
		}
		return indx;
	}
	
	int getSum(char flag,int num,bytearray &I){
		int sum=0;
		if(flag=='r'){
			for (int j = 0; j<I.dim(1); j++)
				sum += I(num, j);
		}
		else{
			for (int i = 0; i<I.dim(0); i++)
				sum += I(i, num);
		}
		
		return sum;
	}
	
	int getSumLR(bytearray &I, int num, int left, int right){
		int sum=0;
		
		for (int j = left; j<=right; j++)
			sum += I(num, j);
		
		return sum;
	}
	
	float getMean(int a[], int N){
		float sum=0;
		for(int i=0;i<N;i++){
			sum=sum+a[i];
		}
		
		return sum/N;
	}
	
	int getMedian(int a[], int N){
		int temp[N];
		for(int  i= 0;i<N;i++){
			temp[i] = a[i];
		}
		sort(temp, temp + N);
		
		return	temp[(int)(ceil((double)(N/2)))];
	}
	
	float getStdDev(int a[], int N){
		if(N>1){
			float var=0;
			float meanVal = getMean(a, N);	
			for (int i=0; i<N; i++){
				var += (a[i] - meanVal) * (a[i] - meanVal);
			}
			
			
			return sqrt(var/(N-1));
		}
		else{
			return 0;
		}
	}
	
	float getStdDevInRange(int a[],int start,int end){
		int N = end-start+1; 
		int aa[N];
		
		for (int i=start, ii=0 ; i<=end; i++, ii++){
			aa[ii] = a[i];
		}
		
		float meanVal = getMean(aa, N);
		
		float var=0;        
		for (int i=0; i<N; i++)
			var += (aa[i] - meanVal) * (aa[i] - meanVal);
		
		return sqrt(var/(N-1));
	}
	
	void cropingImage(bytearray& Simage, bytearray& corped_image, int top,int bottom,int left,int right){
		int numrow=(bottom-top+1);
		int numcol=(right-left+1);
		
		corped_image.resize(numrow,numcol);
		for(int i=top,ni=0;i<=bottom;i++,ni++)
		for(int j=left,nj=0;j<=right;j++,nj++){
			corped_image(ni,nj)=Simage(i,j);
		}
	}
	
	void sortByRows(intarray &arrayToSort, int** sorted_units_info, int arrSize, int numOfElementPerRow, int index){
		// copy the elements of the desired index into the array
		int** tempArray = allocate2DArray(arrSize, 2);
		
		for(int i=0; i<arrSize; i++){
			// store index
			tempArray[i][0] =  i ;
			// store value
			tempArray[i][1] =  arrayToSort(i, index);
		}	
		
		int tempIndex;
		int tempValue;
		
		for(int i = arrSize-1; i>=0; i--){
			// move large values to the top
			for(int j = 0; j<i; j++){
				// if out of order
				if(tempArray[j][1] > tempArray[j+1][1]){
					// then swap
					tempIndex = tempArray[j][0];
					tempValue = tempArray[j][1];
					
					tempArray[j][0] = tempArray[j+1][0];
					tempArray[j][1] = tempArray[j+1][1];
					
					tempArray[j+1][0] =  tempIndex;
					tempArray[j+1][1] = tempValue;
				}
			}
		}
		
		// restore the values of the sorted array
		for( int i=0; i<arrSize; i++){
			index = tempArray[i][0];
			for( int j = 0; j<numOfElementPerRow; j++){
				sorted_units_info[i][j] =  arrayToSort(index, j);
			}
		}
		
		delete2DArray(tempArray, arrSize);
	}
	
	// Overloaded function
	void sortByRows(int** arrayToSort, int** sorted_units_info, int arrSize, int numOfElementPerRow, int index){
		// copy the elements of the desired index into the array
		int** tempArray = allocate2DArray(arrSize, 2);
		
		for(int i=0; i<arrSize; i++){
			// store index
			tempArray[i][0] =  i ;
			// store value
			tempArray[i][1] =  arrayToSort[i][index];
		}	
		
		int tempIndex;
		int tempValue;
		
		for(int i = arrSize-1; i>=0; i--){
			// move large values to the top
			for(int j = 0; j<i; j++){
				// if out of order
				if(tempArray[j][1] > tempArray[j+1][1]){
					// then swap
					tempIndex = tempArray[j][0];
					tempValue = tempArray[j][1];
					
					tempArray[j][0] = tempArray[j+1][0];
					tempArray[j][1] = tempArray[j+1][1];
					
					tempArray[j+1][0] =  tempIndex;
					tempArray[j+1][1] = tempValue;
				}
			}
		}
		
		// restore the values of the sorted array
		for( int i=0; i<arrSize; i++){
			index = tempArray[i][0];
			for( int j = 0; j<numOfElementPerRow; j++){
				sorted_units_info[i][j] =  arrayToSort[index][j];
			}
		}
		
		delete2DArray(tempArray, arrSize);
	}
	
	int** allocate2DArray(int r, int c)
	{
		int** array;
		array = new int *[r];
		for(int i=0;i<r;i++)
			array[i] = new int [c];
		
		return array;
	}
	
	void delete2DArray(int** array, int r){
		for(int i=0; i<r; i++)
			delete[] array[i];
		
		delete[] array;
	}
	
	int getConnectedComponentInformation(bytearray &srcImg, intarray &LabeledWord){
		LabeledWord.resize(srcImg.dim(0), srcImg.dim(1));
		//IsrcImg.resize(srcImg.dim(0), srcImg.dim(1));
		
		// Invert the source Image
		for(int row=0; row<srcImg.dim(0); row++){
			for(int col=0;col<srcImg.dim(1);col++){
				if(srcImg(row, col))
					LabeledWord(row, col) = 0;
				else
					LabeledWord(row, col) = 255;
			}
		}
		
		// get the connected components and extract the units
		label_components(LabeledWord);
		
		return getMaxLabel(LabeledWord);
	} 
	
	void copyDesiredColumn(int** srcArray, int* destArray, int totalRow, int columnToCopy){
		for(int i=0; i<totalRow; i++){
			destArray[i] = srcArray[i][columnToCopy];
		}
	}
	
	void copyDesiredRow(int** srcArray, int* destArray, int totalColumns, int rowToCopy){
		for(int i=0; i<totalColumns; i++){
			destArray[i] = srcArray[rowToCopy][i];
		}
	}
	
	void makeProperBinaryMatLabFormatImage(colib::bytearray &orig_image, colib::bytearray &matlab_page_image){
		bytearray Timage;
		copy(Timage, orig_image);
		
		// make the image proper binary
		make_page_binary_and_black(Timage);
		for(int row=0; row<Timage.dim(0); row++){
			for(int col=0;col<Timage.dim(1);col++){
				if(Timage(row, col)!=0)
					Timage(row, col) = 0;
				else
					Timage(row, col) = 1;
			}
		}
		
		// Build Matlab format Image
		int col=orig_image.dim(0);
		int row=orig_image.dim(1);
		
		matlab_page_image.resize(row, col);
		
		int xx = matlab_page_image.dim(0) - 1;
		for(int x=0; x < matlab_page_image.dim(0); x++){
			for(int y=0; y<matlab_page_image.dim(1); y++){
				matlab_page_image(x, y) = Timage(y, (xx-x)); 
			}
		}
	}
	// <CRBLP>	    // <CRBLP>	    // <CRBLP>	    // <CRBLP>	    // <CRBLP>


}
