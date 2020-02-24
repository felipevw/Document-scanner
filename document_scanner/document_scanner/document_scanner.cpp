// Document_scanner.cpp
//

#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;


// Global variables
Mat result;
Point2f vertices[4];
RotatedRect rotrect;

int roiIndex{};
bool dragging;
int selected_corner_index{};
bool validation_needed = true;
vector< Point2f> roi_corners;


// Function to select and drag points in the GUI
void changePoints(int action, int x, int y, int flags, void* userdata);

int main()
{
	/*// Input image
	cout << "This is a document scanner application.\n"
		"Enter the directory of the image or press h to obtain help." << endl;

	string image;
	cin >> image;
	Mat frame = imread(image, IMREAD_COLOR);
	*/
	Mat frame = imread("test1.jpg", IMREAD_COLOR);

	// Check frame was read
	if (frame.empty())
	{
		cout << "No image was found" << endl;
		std::exit(1);
	}

	// Create output parameters
	const float height_img{ static_cast<float>(frame.rows) };
	const float width_img{ static_cast<float>(frame.cols) };
	const float aspectRatio = width_img / height_img;


	// Preprocessing, bilateralfilter
	Mat bilateralFiltered;
	const int diam{ 60 };
	const double sigmaColor{ 120.0 }, sigmaSpace{ 120.0 };
	bilateralFilter(frame, bilateralFiltered, diam, sigmaColor, sigmaSpace);
	

	// GrabCut operation
	Mat mask;
	Mat bgdModel, fgdModel;
	const int imgReduce{ 5 };
	Rect rect(imgReduce, imgReduce, frame.cols - imgReduce, frame.rows - imgReduce);
	grabCut(bilateralFiltered, mask, rect, bgdModel, fgdModel, 1, GC_INIT_WITH_RECT);
	compare(mask, GC_PR_FGD, mask, CMP_EQ);
	Mat foreground(frame.size(), CV_8UC3, cv::Scalar(0, 0, 0));
	frame.copyTo(foreground, mask);


	// Threshold operation
	Mat imgGray;
	cvtColor(foreground, imgGray, COLOR_BGR2GRAY);
	Mat imgThres;
	threshold(imgGray, imgThres, 100.0, 150.0, THRESH_BINARY);


	// Open operation
	Mat imageMorphOpened;
	const int kSize{ 10 };
	const size_t iterate{ 3 };
	Mat kernel1 = getStructuringElement(cv::MORPH_ELLIPSE,
		cv::Size(kSize, kSize));
	morphologyEx(imgThres, imageMorphOpened, MORPH_OPEN, kernel1, Point(-1, -1), iterate);


	// Find contours operation
	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;
	findContours(imageMorphOpened, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

	
	// Contour with biggest area is the document
	double area{}, big_area{};
	int count{};
	for (size_t i{}; i < contours.size(); i++)
	{
		area = contourArea(contours[i]);
		if (area > big_area)
		{
			big_area = area;
			count = i;
		}
	}


	// Minimum area of the contour, the rotated rect
	rotrect = minAreaRect(contours[count]);
	rotrect.points(vertices);
	

	// Check for unusual values, pass to roi_corners
	for (size_t i{}; i < 4; i++)
	{
		if (vertices[i].x < 0)
			vertices[i].x = 0;

		if (vertices[i].y < 0)
			vertices[i].y = 0;
		roi_corners.push_back(vertices[i]);
	}


	// Contour approximation GUI
	int key{};
	namedWindow("Contour Approximation", WINDOW_AUTOSIZE);
	setMouseCallback("Contour Approximation", changePoints);
	// Press ESC to break the loop
	while (key != 27)
	{
		// Refresh 
		result = frame.clone();

		// Draw bounding lines of the document
		putText(result, "Correct the points, Press ESC to finish",
			Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);
		for (size_t i{}; i < 4; i++)
		{
			line(result, roi_corners[i], roi_corners[(i + 1) % 4], Scalar(0, 255, 0), 2);
			circle(result, roi_corners[i], 25, Scalar(153, 255, 255), -1, LINE_AA);
		}
		imshow("Contour Approximation", result);
		
		key = (char)waitKey(10);
	}


	// Aspect ratio parameters
	const int new_img_width{ 500 };
	int new_img_heigth{ static_cast<int>(500.0f / aspectRatio) };
	Size outDim = Size(new_img_width, new_img_heigth);


	// Destiny points
	vector<Point2f> pts_dst;
	pts_dst.push_back(Point2f(0, 0));									// TopLeft
	pts_dst.push_back(Point2f(0, new_img_heigth - 1));					// BottomLeft
	pts_dst.push_back(Point2f(new_img_width - 1, new_img_heigth - 1));	// BottomRight
	pts_dst.push_back(Point2f(new_img_width - 1, 0));					// TopRight

	// Source points
	vector<Point2f> pts_src;
	pts_src.push_back(roi_corners[1]);									// TopLeft
	pts_src.push_back(roi_corners[0]);									// BottomLeft
	pts_src.push_back(roi_corners[3]);									// BottomRight
	pts_src.push_back(roi_corners[2]);									// TopRight


	// Calculate Homography
	Mat h = findHomography(pts_src, pts_dst);


	// Geometric transformation
	Mat im_warped;
	warpPerspective(frame, im_warped, h, outDim);

	// Scanned Document
	imshow("Warpped Image", im_warped);
	waitKey(0);

	return 0;
}


void changePoints(int action, int x, int y, int flags, void* userdata)
{
	// Action when left button is pressed
	if (roi_corners.size() == 4)
	{
		for (int i = 0; i < 4; ++i)
		{
			// Action when mouse is close to
			if ((action == EVENT_LBUTTONDOWN) & ((abs(roi_corners[i].x - x) < 20)) & (abs(roi_corners[i].y - y) < 20))
			{
				selected_corner_index = i;
				dragging = true;
			}
		}
	}
	else if (action == EVENT_LBUTTONDOWN)
	{
		roi_corners.push_back(Point2f((float)x, (float)y));
		validation_needed = true;
	}

	// Action when left button is released
	if (action == EVENT_LBUTTONUP)
	{
		dragging = false;
	}

	// Action when left button is pressed and mouse has moved over the window
	if ((action == EVENT_MOUSEMOVE) && dragging)
	{
		roi_corners[selected_corner_index].x = (float)x;
		roi_corners[selected_corner_index].y = (float)y;
		validation_needed = true;
	}
}