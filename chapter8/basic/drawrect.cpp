#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace cv;

int main()
{
    Mat image=Mat::zeros(300,400,CV_8UC3);
    image.setTo(Scalar(255,255,255));
    Scalar color(0,255,255);
    Point p1(10,10),p2(100,100),p3(220,10);
    Size size(100,100);
    Rect rect1(110,10,100,100);
    Rect rect2(p3,size);
    rectangle(image,p1,p2,color,2);
    rectangle(image, rect1,color,2);
    rectangle(image, rect2,color,2);


    imshow("Draw Rect",image);
    waitKey(0);
    return 0;
}
