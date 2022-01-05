 
/**
*/
#include <ctime>
#include <fstream>
#include <iostream>
#include <vector>
#include <raspicam/raspicam_cv.h>
#include <zbar.h>
#include <opencv2/imgproc/imgproc.hpp>

using namespace std;

typedef struct
{
    string type;
    string data;
    vector <cv::Point> location;
}decodedObject;


cv::Mat adjust_contrast_brightness(cv::Mat image, float alpha, float beta) 
{
    cv::Mat new_image = cv::Mat::zeros( image.size(), image.type() );
    
    for( int y = 0; y < image.rows; y++ ) {
        for( int x = 0; x < image.cols; x++ ) {
            for( int c = 0; c < image.channels(); c++ ) {
                new_image.at<cv::Vec3b>(y,x)[c] =
                cv::saturate_cast<uchar>( alpha*image.at<cv::Vec3b>(y,x)[c] + beta );
            }
        }
    }

    return new_image;
}

cv::Mat gamma_correction(cv::Mat image, float gamma)
{
    cv::Mat lookUpTable(1, 256, CV_8U);
    uchar* p = lookUpTable.ptr();
    for( int i = 0; i < 256; ++i)
        p[i] = cv::saturate_cast<uchar>(pow(i / 255.0, gamma) * 255.0);
    cv::Mat res = image.clone();
    cv::LUT(image, lookUpTable, res);
    
    return res;
}
// Display barcode and QR code location
void display(cv::Mat &im, vector<decodedObject>&decodedObjects)
{
    // Loop over all decoded objects
    for(unsigned int i = 0; i < decodedObjects.size(); i++)
    {
        vector<cv::Point> points = decodedObjects[i].location;
        vector<cv::Point> hull;

        // If the points do not form a quad, find convex hull
        if(points.size() > 4)
            cv::convexHull(points, hull);
        else
            hull = points;

        // Number of points in the convex hull
        int n = hull.size();

        for(int j = 0; j < n; j++)
        {
            cv::line(im, hull[j], hull[ (j+1) % n], cv::Scalar(255,0,0), 3);
        }
    }
}

void decode(cv::Mat &im, vector<decodedObject>&decodedObjects, int nb_frames)
{

    // Create zbar scanner
    zbar::ImageScanner scanner;

    // Configure scanner
    scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 0);
    scanner.set_config(zbar::ZBAR_QRCODE, zbar::ZBAR_CFG_ENABLE, 1);

    // Convert image to grayscale
    cv::Mat imGray, blur;
    // imGray = adjust_contrast_brightness(im, 1, 0);
    cv::cvtColor(im, imGray, cv::COLOR_BGR2GRAY);
    imGray = gamma_correction(imGray, 0.4);
    // cv::GaussianBlur(imGray, imGray, cv::Size(3, 3), 0);
    // cv::Mat kernel = (cv::Mat_<double>(3,3) << -1,-1,-1, -1,9,-1, -1,-1,-1);
    // cv::filter2D(imGray, imGray, -1, kernel);
    // cv::threshold(imGray, imGray, 125, 255, cv::THRESH_OTSU);
    // cv::adaptiveThreshold(imGray, imGray, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 11, 1);
    // cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    // cv::erode(imGray, imGray, kernel);
    // cv::morphologyEx(imGray, imGray, cv::MORPH_CLOSE, kernel);
    
    // cv::imshow("frame", imGray);
    
    // Wrap image data in a zbar image
    zbar::Image image(im.cols, im.rows, "Y800", (uchar *)imGray.data, im.cols * im.rows);

    // Scan the image for barcodes and QRCodes
    int res = scanner.scan(image);
    
    if (res > 0) {
        // Print results
        for(zbar::Image::SymbolIterator symbol = image.symbol_begin(); symbol != image.symbol_end(); ++symbol)
        {
            decodedObject obj;

            obj.type = symbol->get_type_name();
            obj.data = symbol->get_data();
            // Obtain location

            for(int i = 0; i< symbol->get_location_size(); i++)
            {
                obj.location.push_back(cv::Point(symbol->get_location_x(i),symbol->get_location_y(i)));
            }

            // Print type and data
            cout << nb_frames << endl;
            cout << "Type : " << obj.type << endl;
            cout << "Data : " << obj.data << endl << endl;
            decodedObjects.push_back(obj);
        }
        
        display(im, decodedObjects);
    }
}
 
int main ( int argc,char **argv ) {
    raspicam::RaspiCam_Cv Camera; //Cmaera object
    //Open camera 
    cout<<"Opening Camera..."<<endl;
    if ( !Camera.open()) {
        cerr<<"Error opening camera"<<endl;return -1;
    }
    cout << endl << "======================================" << endl;
    cout << "Camera(" << Camera.getId() << ")Info: " << endl;
    cout << "Size: (" << Camera.getWidth() << " x " << Camera.getHeight() << ")" << endl;
    cout << "FPS: " << Camera.getFrameRate() << endl;
    cout << "Brightness: " << Camera.getBrightness() << endl;
    cout << "ISO: " << Camera.getISO() << endl;
    cout << "Shutter Speed: " << Camera.getShutterSpeed() << endl;
    cout << "======================================" << endl << endl;;
    
    //wait a while until camera stabilizes
    cout<<"Sleeping for 1 secs"<<endl;
    enum {SECS_TO_SLEEP = 1, NSEC_TO_SLEEP = 125};
    struct timespec remaining, request = {SECS_TO_SLEEP, NSEC_TO_SLEEP};
    nanosleep(&request, &remaining);
    // unsigned char *data=new unsigned char[Camera.getImageTypeSize(format)];
    cv::Mat data;
    cout << "Show Preivew..." << endl;
    int nb_frames = 0;
    while(1) {
        //capture
        nb_frames++;
        Camera.grab();
        //extract the image in rgb format
        Camera.retrieve(data);//get camera image
        
        vector<decodedObject> decodedObjects;

        // Find and decode barcodes and QR codes
        cv::resize(data, data, cv::Size(720, 720));
        decode(data, decodedObjects, nb_frames);
        cv::imshow("preivew", data);
        if (cv::waitKey(5) >= 0) {
            break;
        }
    }
    
    // save
    // std::ofstream outFile ( "raspicam_image.ppm",std::ios::binary );
    // outFile<<"P6\n"<<Camera.getWidth() <<" "<<Camera.getHeight() <<" 255\n";
    // outFile.write ( ( char* ) data, Camera.getImageTypeSize ( raspicam::RASPICAM_FORMAT_RGB ) );
    // cout<<"Image saved at raspicam_image.ppm"<<endl;
    // free resrources    
    // delete data;
    return 0;
}
