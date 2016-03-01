
#include <sstream>
#include <string>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <math.h>

#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/core/core.hpp"

#define BAUDRATE B115200
//#define ARDUINOPORT "/dev/ttyUSB0"
#define ARDUINOPORT "/dev/ttyACM0"

using namespace cv;
using namespace std;

//width and height
int WIDTH = 500;
int HEIGHT = 500;

int fd = 0;
struct termios TermOpt;

int stepPermmX;
int stepPermmY;
int stepPermmZ;

int jeda = 0;
int speed;
int key;

int spindle;

double pointX;
double pointY = HEIGHT;
double pointZ;
double del, accl;
double lastpointX;
double lastpointY = HEIGHT;
double lastpointZ;

double drawpointX;
double drawpointY;
double drawpointZ;
double drawlastpointX;
double drawlastpointY;
double drawlastpointZ; 
double difX, difY, difZ;
double maxStep, stepX, stepY, stepZ;
double intstepX, intstepY, intstepZ, lastintstepX, lastintstepY, lastintstepZ;
double drawmaxStep;
double drawstepX;
double drawstepY;
double reversedrawstepY = HEIGHT*stepPermmY;
double drawstepZ;
double lastdrawstepX;
double lastdrawstepY = HEIGHT*stepPermmY;
double lastdrawstepZ;
double deep;

int warna;

//++++++++++ function convertion integer to string ++++++++++
string intToString(int number){


	std::stringstream ss;
	ss << number;
	return ss.str();
}
//---------- end function convertion integer to string ----------

//++++++++++ function convertion string to double ++++++++++
double StringToDouble(string s){

	double d;
	std::stringstream ss(s);
	ss >> d;
	return d;
}
//---------- end function convertion string to double ----------

void bacaKonfig(){
	string baris;
	ifstream Konfig ("config.txt");
	if(Konfig.is_open()){
		while(getline(Konfig, baris, ' ')){
			if(baris.substr(0,1)=="X" ){
				stepPermmX = int(StringToDouble(baris.substr(1)));
			}
			if(baris.substr(0,1)=="Y" ){
				stepPermmY = int(StringToDouble(baris.substr(1)));
			}
			if(baris.substr(0,1)=="Z" ){
				stepPermmZ = int(StringToDouble(baris.substr(1)));
			}
			if(baris.substr(0,1)=="S" ){
				speed = int(StringToDouble(baris.substr(1)));
			}
		}//while(getline(myfile, baris, ' '))
	}//if(Konfig.is_open()){
	Konfig.close();
}

void simpanKonfig(){
	ofstream outfile;
	outfile.open ("config.txt");
	outfile << "X"<< stepPermmX << " Y" << stepPermmY << " Z" << stepPermmZ << " S" << speed ;
	outfile.close();
}

//+++++++++++++++++++++++ Start readport ++++++++++++++++++++++++++
char  readport(void){
  int n;
	char buff;
  n = read(fd, &buff, 1);
  if(n > 0){
    return buff;
  }
  return 0;
}
//------------------------ End readport ----------------------------------

//+++++++++++++++++++++++ Start sendport ++++++++++++++++++++++++++
void sendport(unsigned char ValueToSend){
  int n;

  n = write(fd, &ValueToSend, 1);
  
  if (n < 0){
    cout<< "write() of value failed!\n";
  }
  //else{
    //while(readport() != 'r');
  //}
  
}
//------------------------ End sendport ----------------------------------

//+++++++++++++++++++++++ Start openport ++++++++++++++++++++++++++
void openport(void){
    
    fd = open(ARDUINOPORT, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)  {
      cout << "init_serialport: tidak dapat membuka port\n";
    }
    
    if (tcgetattr(fd, &TermOpt) < 0) {
      cout << "init_serialport: tidak mendapatkan attribut term\n";
    }
    speed_t brate = BAUDRATE; // let you override switch below if needed

    cfsetispeed(&TermOpt, brate);
    cfsetospeed(&TermOpt, brate);

    // 8N1
    TermOpt.c_cflag &= ~PARENB;
    TermOpt.c_cflag &= ~CSTOPB;
    TermOpt.c_cflag &= ~CSIZE;
    TermOpt.c_cflag |= CS8;
    // no flow control
    TermOpt.c_cflag &= ~CRTSCTS;

    TermOpt.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
    TermOpt.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl

    TermOpt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // make raw
    TermOpt.c_oflag &= ~OPOST; // make raw

    // see: http://unixwiz.net/techtips/termios-vmin-vtime.html
    TermOpt.c_cc[VMIN]  = 0;
    TermOpt.c_cc[VTIME] = 20;
    
    if( tcsetattr(fd, TCSANOW, &TermOpt) < 0) {
      //ErrorText("init_serialport: Couldn't set term attributes");
    }

} 
//------------------------ End openport ----------------------------------

//++++++++++ function interpolation ++++++++++
double interpolasi(double x, double x1, double x2, double y1, double y2)
{
  double y = (y2-y1)*(x-x1)/(x2-x1)+y1;
  return y;
}
//---------- end interpolation ----------

//++++++++++++++ procedure drawGcode +++++++++++++++++++++++
void execute(int mode){

	//image
	Mat image = Mat::zeros(HEIGHT, WIDTH, CV_8UC3);
	
	//open file
	string baris, baris1;
	ifstream myfile ("a.nc");
	if(myfile.is_open()){
		while(getline(myfile, baris)){
			stringstream streamBaris (baris);
			while(getline(streamBaris, baris1, ' ')){
				//cout << baris1 << '\n';
				if(baris1.substr(0,1)=="X"){
					pointX = round(StringToDouble(baris1.substr(1)));
					//cout << "X" << pointX << '\n';
				}
				if(baris1.substr(0,1)=="Y"){
					pointY = round(StringToDouble(baris1.substr(1)));
					pointY = interpolasi(pointY, 0, HEIGHT, HEIGHT, 0);
					//cout << "Y" << pointY << '\n';
				}
			}//while (getline(streamBaris, baris1, ' '))
			cout << "Draw " << "X" << pointX << " Y" << pointY << '\n' ;
			if(pointX != lastpointX || pointY != lastpointY){
				line(image, Point(lastpointX, lastpointY), Point(pointX, pointY), Scalar(255,0,0), 1, 8);
				imshow("SuryaProCell-CNC", image);
			}
			lastpointX = pointX;
			lastpointY = pointY;
		}//while(getline(myfile, baris))
		myfile.close();
	}//if(myfile.is_open())
	else{
		cout << "Tidak dapat membuka file" << '\n';
	}//else if(myfile.is_open())

if(mode == 2){

	// image
	Mat image2;					
	image.copyTo(image2);

	//open file
	string baris, baris1;
	ifstream myfile ("a.nc");
	if(myfile.is_open()){
		while(getline(myfile, baris)){
			stringstream streamBaris (baris);
			while(getline(streamBaris, baris1, ' ')){
				if(baris1=="M3"){
					spindle = 1;
					sendport(1);
					while(readport() != '1');
					cout << "Spindle ON" << '\n';
				}
				if(baris1=="M5"){					
					spindle = 0;
					sendport(2);
					while(readport() != '2');
					cout << "Spindle OFF" << '\n';
				}
				if(baris1.substr(0,1)=="X"){
					drawpointX = round(StringToDouble(baris1.substr(1)) * stepPermmX);
					//cout << "X" << pointX << '\n';
				}
				if(baris1.substr(0,1)=="Y"){
					drawpointY = round(StringToDouble(baris1.substr(1)) * stepPermmY);
					//drawpointY = interpolasi(drawpointY, 0, HEIGHT*stepPermmY, HEIGHT*stepPermmY, 0);
					//cout << "Y" << pointY << '\n';
				}
				if(baris1.substr(0,1)=="Z"){
					drawpointZ = round(StringToDouble(baris1.substr(1))  * stepPermmZ);
					//cout << "Z" << pointZ << '\n';
				}
				
			}//while(getline(streamBaris, baris1, ' '))
			cout << "X" << drawpointX << " Y" << drawpointY << " Z" << drawpointZ << " S" << spindle << '\n' ;
			if(drawpointX != drawlastpointX || drawpointY != drawlastpointY || drawpointZ != drawlastpointZ){ 
				difX = abs(drawpointX - drawlastpointX);
				difY = abs(drawpointY - drawlastpointY);
				difZ = abs(drawpointZ - drawlastpointZ);
				
				maxStep = max(difX, difY);
				maxStep = max(maxStep, difZ);
				//cout << maxStep << '\n';
				int i = 1;
				while(i <= maxStep){
					stepX = interpolasi(i, 0, maxStep, drawlastpointX, drawpointX);
					stepY = interpolasi(i, 0, maxStep, drawlastpointY, drawpointY);
					stepZ = interpolasi(i, 0, maxStep, drawlastpointZ, drawpointZ);

					intstepX = round(stepX);
					intstepY = round(stepY);
					intstepZ = round(stepZ);

					if(intstepX>lastintstepX){
						sendport(4);
						//waitKey(1);
						//while(readport() != 'X');
						lastintstepX = intstepX;
					}
					if(intstepX<lastintstepX){
						sendport(8);
						//waitKey(1);
						//while(readport() != 'X');
						lastintstepX = intstepX;
					}
					if(intstepY>lastintstepY){
						sendport(16);
						//waitKey(1);
						//while(readport() != 'Y');
						lastintstepY = intstepY;
					}
					if(intstepY<lastintstepY){
						sendport(32);
						//waitKey(1);
						//while(readport() != 'Y');
						lastintstepY = intstepY;
					}
					if(intstepZ>lastintstepZ){
						sendport(64);
						//waitKey(1);
						//while(readport() != 'Z');
						lastintstepZ = intstepZ;
					}
					if(intstepZ<lastintstepZ){
						sendport(128);
						//waitKey(1);
						//while(readport() != 'Z');
						lastintstepZ = intstepZ;
					}
					cout << "X" << lastintstepX << " Y" << lastintstepY << " Z" << lastintstepZ << " S" << spindle << '\n';

					drawstepX = interpolasi(stepX, 0, WIDTH*stepPermmY, 0, WIDTH);
					drawstepY = interpolasi(stepY, 0, HEIGHT*stepPermmY, 0, HEIGHT);
					reversedrawstepY = interpolasi(drawstepY, 0, HEIGHT, HEIGHT, 0);
					drawstepZ = interpolasi(stepZ, 0, HEIGHT*stepPermmZ, 0, HEIGHT);

					deep = interpolasi(drawstepZ, 0, -10, 255, 0);

					//image
					Mat image3;					
					image2.copyTo(image3);

					line(image2, Point(lastdrawstepX, lastdrawstepY), Point(drawstepX, reversedrawstepY), Scalar(deep,deep,255), 2, 8);
					
					if(spindle==1){
						warna = 0;
					}else{
						warna = 255;
					}

					circle(image3, Point(drawstepX,reversedrawstepY), 10, Scalar(warna,255,warna), 2);
					putText(image3, "X"+intToString(drawstepX) + " Y" + intToString(drawstepY) + " Z" + intToString(drawstepZ), Point(drawstepX+10,reversedrawstepY-10), FONT_HERSHEY_COMPLEX, 0.5, Scalar(0, 255, 0), 1, 8);

					imshow("SuryaProCell-CNC", image2);
					imshow("SuryaProCell-CNC", image3);
					//cout << "X" << drawstepX << " Y" << drawstepY << '\n';

					lastdrawstepX = drawstepX;
					lastdrawstepY = reversedrawstepY;
					lastdrawstepZ = drawstepZ;

					//limit bottom speed
					if(speed == 0){
						speed = 1;
					}
					del = 1000/speed; // ms per step

					//akselerasi
					if(maxStep >= 20){
						if(i<=20){
							accl = interpolasi(i, 0, 20, del*2, del);
						}else if(i>=maxStep-20){
							accl = interpolasi(i, maxStep-20, maxStep, del, del*2);
						}else{
							accl = del;
						}
					}else{
						accl = del*2;
					}

					//pause
					key = waitKey(accl);
					if(key == 112) { //jika tombol 'p' ditekan
						jeda = 1;
						sendport(2); //matikan bor
						circle(image3, Point(drawstepX,reversedrawstepY), 10, Scalar(255,255,255), 2);
						imshow("SuryaProCell-CNC", image3);	
					}
					
					while(jeda == 1){
						key = waitKey(100);
						if(key == 115) { //jika tombol 's' ditekan
							jeda = 0;
							sendport(1); //hidupkan bor
							circle(image3, Point(drawstepX,reversedrawstepY), 10, Scalar(0,255,0), 2);
							imshow("SuryaProCell-CNC", image3);
							waitKey(1000);	
						}
						if (key == 27) { //esc
            						cout << "Keluar" << endl;
            						exit(0); 
       						}
					}

					i++;				
					
				}//while(i <= maxStep)
			drawlastpointX = drawpointX;
			drawlastpointY = drawpointY;
			drawlastpointZ = drawpointZ;
			}//if(drawpointX != drawlastpointX || drawpointY != drawlastpointY || drawpointZ != drawlastpointZ)
		}//while(getline(myfile, baris))
		myfile.close();
	}//if(myfile.is_open())
}//if(mode == 2)
}

//---------------- end drawGcode ------------------------------

//++++++++++ main ++++++++++
int main(){

	system("clear");
	bacaKonfig();
	openport();

	/*open serial
	FILE* serial = fopen("/dev/ttyACM0", "w");
	if (serial == 0) {
		cout << "Tidak dapat membuka serial port" << '\n';
	}*/
	
	while(readport() != 'R');
	cout << "Arduino Ready\n";

	//image
	Mat imageCmd = Mat::zeros(HEIGHT+200, WIDTH+200, CV_8UC3);
	putText(imageCmd, "----- MENU -----", Point(10,20), FONT_HERSHEY_COMPLEX, 0.75, Scalar(0, 255, 0), 1, 8);
	putText(imageCmd, "d : Menggerakkan X axis ke arah positif", Point(10,40), FONT_HERSHEY_COMPLEX, 0.5, Scalar(0, 255, 0), 1, 8);
	putText(imageCmd, "a : Menggerakkan X axis ke arah negatif", Point(10,60), FONT_HERSHEY_COMPLEX, 0.5, Scalar(0, 255, 0), 1, 8);
	putText(imageCmd, "w : Menggerakkan Y axis ke arah positif", Point(10,80), FONT_HERSHEY_COMPLEX, 0.5, Scalar(0, 255, 0), 1, 8);
	putText(imageCmd, "x : Menggerakkan Y axis ke arah negatif", Point(10,100), FONT_HERSHEY_COMPLEX, 0.5, Scalar(0, 255, 0), 1, 8);
	putText(imageCmd, "q : Menggerakkan Z axis ke arah positif", Point(10,120), FONT_HERSHEY_COMPLEX, 0.5, Scalar(0, 255, 0), 1, 8);
	putText(imageCmd, "z : Menggerakkan Z axis ke arah negatif", Point(10,140), FONT_HERSHEY_COMPLEX, 0.5, Scalar(0, 255, 0), 1, 8);
	putText(imageCmd, "e : Menghidupkan Bor", Point(10,160), FONT_HERSHEY_COMPLEX, 0.5, Scalar(0, 255, 0), 1, 8);
	putText(imageCmd, "c : Mematikan Bor", Point(10,180), FONT_HERSHEY_COMPLEX, 0.5, Scalar(0, 255, 0), 1, 8);
	putText(imageCmd, "s : Mulai mesin", Point(10,200), FONT_HERSHEY_COMPLEX, 0.5, Scalar(0, 255, 0), 1, 8);
	putText(imageCmd, "p : Jeda Mesin", Point(10,220), FONT_HERSHEY_COMPLEX, 0.5, Scalar(0, 255, 0), 1, 8);
	putText(imageCmd, "f : Simpan Konfigurasi", Point(10,240), FONT_HERSHEY_COMPLEX, 0.5, Scalar(0, 255, 0), 1, 8);
	putText(imageCmd, "esc : Keluar", Point(10,260), FONT_HERSHEY_COMPLEX, 0.5, Scalar(0, 255, 0), 1, 8);
	imshow("Command", imageCmd);

	execute(1);

	//create a window called "Control"
	//namedWindow("Control", CV_WINDOW_AUTOSIZE);
	namedWindow("Config", CV_WINDOW_NORMAL);

	//Create trackbars in "Control" window
	cvCreateTrackbar("Speed(step/detik)", "Config", &speed, 100); //speed (0 - 100)
	cvCreateTrackbar("X(step/mm)", "Config", &stepPermmX, 1000);
	cvCreateTrackbar("Y(step/mm)", "Config", &stepPermmY, 1000);
	cvCreateTrackbar("Z(step/mm)", "Config", &stepPermmZ, 1000);

    	while (true){

		key = waitKey(100);

		if(key == 101){ //tombol 'e' ditekan
			sendport(1); // spindle ON
		}
		if(key == 99) { //tombol 'c' ditekan. 
			sendport(2); // spindle OFF
		}
		if(key == 100){ //tombol 'd' ditekan
			sendport(4); // X axis ++
		}
		if(key == 97) { //tombol 'a' ditekan. 
			sendport(8); // X axis --
		}
		if(key == 119) { //tombol 'w' ditekan. 
			sendport(16); // Y axis ++
		}
		if(key == 120) { //tombol 'x' ditekan. 
			sendport(32); // Y axis --
		}
		if(key == 113) { //tombol 'q' ditekan. 
			sendport(64); // Z axis ++
		}
		if(key == 122) { //tombol 'z' ditekan. 
			sendport(128); // Z axis --
		}
		if(key == 115) { // tombol 's' ditekan
			execute(2); //start arduino
		}
		if(key == 102) { // tombol 'f' ditekan
			simpanKonfig(); //simpan konfig
		}
		if (key == 27) { //esc
            		cout << "Keluar" << endl;
            		break; 
       		}
	
        } //end while (true)
	
   	return 0;

}
//---------- end Main ----------