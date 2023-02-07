// testApplication.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

//todo.实现日志储存功能
//	+记录运行时间
//
//长期目标:用class封装程序逻辑

//已知问题:
//1.当脚本文件编码不是GBK时echo输出的汉字可能为乱码
//	目前并不打算处理这个问题(确实不会字符串转码)

#include <iostream>
#include <opencv2/opencv.hpp>
#include <windows.h>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>
#include <fstream>
#include <conio.h>
#include <cstring>
#include <vector>
#include <stack>
#include <ctime>
#include <cmath>
#pragma comment(lib,"winmm.lib")

#define Keydown(key) ((GetAsyncKeyState(key) && 0x8000)?1:0)

#define Pressbreak() (Keydown(17) && Keydown(18) && Keydown(16))

using namespace cv;
using namespace std;


const int SCANGAP = 100;//截屏间隔(ms)
const double PICACC = 0.95;//图片识别相似度(0-1,越大要求越精确)
const bool MAKELOG = false;//是否输出日志文件(暂未使用)

vector<int> buff[5];//opt
vector<string> room;//辅助存储字符串
/*
press A	//单击A键							1
keydown B//按下B键							2
keyup B//弹起B键							3

click L //单击鼠标左键						4
mousedown R//按下鼠标右键					5
mouseup M//抬起鼠标中键						6
mousemove x y//移动鼠标位置					7

wait 1000//暂停1000秒						8
rest //暂停直到按下ctrl+alt					9
waitkey A//暂停直到按下A键					10
rwait 1000 20//随机暂停1000-(1000+20)ms		11

waitscreenscan a.jpg						12
//当屏幕中出现指定图片内容后继续

loop 10//运行循环10次						13
endloop//循环结束							14

echo Hallo world//输出指定字符串			15

note hello 2023//注释						16			

*/


bool bk[500];//检测按键切换
int keyList[500],keyNum=0;
int oemMark[500],oemBreak[500];//键盘对应oem码的按下和弹起
stack<int> loopmark,loopcnt;//记录循环次数和对应行数


void popError(int err, int var=0)//错误出现
{
	if (err == 1)
	{
		cout << "错误:文件不存在" << endl;
	}
	else if (err == 2)
	{
		cout << "错误:文件类型不匹配" << endl;
	}
	else if (err == 3)
	{
		cout << "错误:打开的不是一个文件" << endl;
	}
	else if (err == 4)
	{
		cout << "错误:第" << var << "条指令参数错误" << endl;
	}
	else if (err == 5)
	{
		cout << "错误:第" << var << "条指令名称错误" << endl;
	}
	else if (err == 6)
	{
		cout << "错误:文件" << room[var] << "不存在或无法读写" << endl;
	}
	else if (err == 7)
	{
		cout << "错误:第" << var << "条指令没有相匹配的loop语句" << endl;
	}
	else if (err == 8)
	{
		cout << "错误:第" << var << "条指令没有相匹配的endloop语句" << endl;
	}
	else if (err == 9)
	{
		cout << "错误:手动中断脚本运行" << endl;
	}
	else
	{
		cout << "错误:未知错误" << endl;
	}
	system("pause");
	exit(-1);
}

void buffload(int a, int b = 0, int c = 0, int d = 0, int e = 0)
{
	buff[0].push_back(a);
	buff[1].push_back(b);
	buff[2].push_back(c);
	buff[3].push_back(d);
	buff[4].push_back(e);
}

void makeKeyList()//导入需要定位的按键列表
{
	for (int i = 65; i <= 90; i++)
	{
		keyList[keyNum++] = i;//A-Z
	}
	for (int i = '0'; i <= '9'; i++)
	{
		keyList[keyNum++] = i;//0-9
	}
	for (int i = '0'+48; i <= '9'+48; i++)
	{
		keyList[keyNum++] = i;//小键盘0-9
	}
	keyList[keyNum++] = 13;//Enter
	keyList[keyNum++] = 9;//Tab
	keyList[keyNum++] = 8;//Backspace
	keyList[keyNum++] = 1;//鼠标左键
	keyList[keyNum++] = 2;//鼠标右键
	keyList[keyNum++] = 4;//鼠标中键
	
}

int makeOem()//导入按键扫描码
{
	char tmp[] = "QWERTYUIOPASDFGHJKLZXCVBNM";
	for (int i = '0'; i <= '9'; i++)//数字键盘
	{
		oemMark[i] = 0x02 + i - '0';
		oemBreak[i] = 0x82 + i - '0';
	}

	int cnt = 0x47, cnt2 = 0xc7;
	for (int i = '7'+48;; i++)//小键盘
	{
		oemMark[i] = cnt++;
		oemBreak[i] = cnt2++;
		if ((i - '0' - 48) == 3)break;
		if ((i - '0' - 48) % 3 == 0)
		{
			i -= 6;
		}
	}
	oemMark['0'+48] = cnt++;
	oemBreak['0'+48] = cnt2++;

	cnt = 0x10;
	cnt2 = 0x90;
	for (int i = 0; i < strlen(tmp); i++)//字母键
	{
		if (tmp[i] == 'A')
		{
			cnt = 0x1e;
			cnt2 = 0x9e;
		}
		else if (tmp[i] == 'Z')
		{
			cnt = 0x2c;
			cnt2 = 0xac;
		}
		oemMark[tmp[i]] = cnt++;
		oemBreak[tmp[i]] = cnt2++;
	}
	oemMark[13] = 0x1c;
	oemBreak[13] = 0x9c;
	oemMark[9] = 0x0f;
	oemBreak[9] = 0x8f;
	oemMark[8] = 0x0e;
	oemBreak[8] = 0x8e;

	return 0;
}

string keyName(int key)
{
	string rt="";
	if (key >= 'A' && key <= 'Z')
	{
		rt += char(key);
	}
	if (key >= '0' && key <= '9')
	{
		rt += char(key);
	}
	if ((key >= ('0' + 48)) && (key <= ('9' + 48)))
	{
		rt += "num";
		rt+=char(key - 48);
	}
	if (key == 1)
	{
		rt += "mouseL";
	}
	if (key == 2)
	{
		rt += "mouseR";
	}
	if (key == 4)
	{
		rt += "mouseM";
	}
	if (key == 13)
	{
		rt += "enter";
	}
	if (key == 9)
	{
		rt += "backspace";
	}
	if (key == 8)
	{
		rt += "tab";
	}
	return rt;
}

int keyCode(string name)
{
	if (name.size() == 1)//单个字符
	{
		char ch = name[0];
		if (ch >= 'A' && ch <= 'Z')
		{
			return ch;
		}
		if (ch >= 'a' && ch <= 'z')
		{
			return 'A' + ch - 'a';
		}
		if (ch >= '0' && ch <= '9')
		{
			return ch;
		}
	}
	else//多个字符
	{
		if (name.substr(0, 3) == "num")
		{
			char ch = name[3];
			if (ch >= '0' && ch <= '9')
			{
				return ch + 48;
			}
		}
		if (name == "enter" || name == "Enter" || name == "ENTER")
		{
			return 13;
		}
		if (name == "tab" || name == "Tab" || name == "TAB")
		{
			return 9;
		}
		if (name == "backspace" || name == "Backspace" || name == "BACKSPACE")
		{
			return 8;
		}
		return -1;
	}
}

void GetStringSize(HDC hDC, const char* str, int* w, int* h)
{
	SIZE size;
	GetTextExtentPoint32A(hDC, str, strlen(str), &size);
	if (w != 0) *w = size.cx;
	if (h != 0) *h = size.cy;
}

void PutTextZH(Mat& dst, const char* str, Point org, Scalar color, int fontSize,
	const char* fn, bool isalic, bool underline)
{
	CV_Assert(dst.data != 0 && (dst.channels() == 1 || dst.channels() == 3));
	int x, y, r, b;
	if (org.x > dst.cols || org.y > dst.rows)
	{
		return;
	}
	x = org.x < 0 ? -org.x : 0;
	y = org.y < 0 ? -org.y : 0;

	LOGFONTA lf;
	lf.lfHeight = -fontSize;
	lf.lfWidth = 0;
	lf.lfEscapement = 0;
	lf.lfOrientation = 0;
	lf.lfWeight = 5;
	lf.lfItalic = isalic;//斜体
	lf.lfUnderline = underline;//下划线
	lf.lfStrikeOut = 0;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfOutPrecision = 0;
	lf.lfClipPrecision = 0;
	lf.lfQuality = PROOF_QUALITY;
	lf.lfPitchAndFamily = 0;
	strcpy_s(lf.lfFaceName, fn);//字体

	HFONT hf = CreateFontIndirectA(&lf);
	HDC hDC = CreateCompatibleDC(0);
	HFONT hOldFont = (HFONT)SelectObject(hDC, hf);

	int strBaseW = 0, strBaseH = 0;
	int singleRow = 0;
	char buf[1 << 12];
	strcpy_s(buf, str);
	char* bufT[1 << 12];//分割字符串后剩余的字符

	//处理多行
	{
		int nnh = 0;//行数
		int cw, ch;
		const char* ln = strtok_s(buf, "\n", bufT);//分割第一个\n前的字符串
		while (ln != 0)
		{
			GetStringSize(hDC, ln, &cw, &ch);
			strBaseW = max(strBaseW, cw);
			strBaseH = max(strBaseH, ch);

			ln = strtok_s(0, "\n", bufT);//返回下一个\n前的字符串
			nnh++;
		}
		singleRow = strBaseH;//单行高度
		strBaseH *= nnh;//总高度
	}

	if (org.x + strBaseW < 0 || org.y + strBaseH < 0)
	{
		SelectObject(hDC, hOldFont);
		DeleteObject(hf);
		DeleteObject(hDC);
		return;
	}
	r = org.x + strBaseW > dst.cols ? dst.cols - org.x - 1 : strBaseW - 1;
	b = org.y + strBaseH > dst.rows ? dst.rows - org.y - 1 : strBaseH - 1;
	org.x = org.x < 0 ? 0 : org.x;
	org.y = org.y < 0 ? 0 : org.y;

	BITMAPINFO bmp = { 0 };
	BITMAPINFOHEADER& bih = bmp.bmiHeader;
	int strDrawLineStep = strBaseW * 3 % 4 == 0 ? strBaseW * 3 : (strBaseW * 3 + 4 - (strBaseW * 3 % 4));

	bih.biSize = sizeof(BITMAPINFOHEADER);
	bih.biWidth = strBaseW;
	bih.biHeight = strBaseH;
	bih.biPlanes = 1;
	bih.biBitCount = 24;
	bih.biCompression = BI_RGB;
	bih.biSizeImage = strBaseH * strDrawLineStep;
	bih.biClrUsed = 0;
	bih.biClrImportant = 0;

	void* pDibData = 0;
	HBITMAP hBmp = CreateDIBSection(hDC, &bmp, DIB_RGB_COLORS, &pDibData, 0, 0);

	CV_Assert(pDibData != 0);

	HBITMAP hOldBmp = (HBITMAP)SelectObject(hDC, hBmp);

	SetTextColor(hDC, RGB(255, 255, 255));
	SetBkColor(hDC, 0);

	strcpy_s(buf, str);
	const char* ln = strtok_s(buf, "\n", bufT);//分割第一个\n前的字符串
	int outTextY = 0;
	while (ln != 0)
	{
		TextOutA(hDC, 0, outTextY, ln, strlen(ln));
		outTextY += singleRow;
		ln = strtok_s(0, "\n", bufT);//返回下一个\n前的字符串
	}
	uchar* dstData = (uchar*)dst.data;
	int dstStep = dst.step / sizeof(dstData[0]);
	unsigned char* pImg = (unsigned char*)dst.data + org.x * dst.channels() + org.y * dstStep;
	unsigned char* pStr = (unsigned char*)pDibData + x * 3;

	for (int tty = y; tty <= b; ++tty)
	{
		unsigned char* subImg = pImg + (tty - y) * dstStep;
		unsigned char* subStr = pStr + (strBaseH - tty - 1) * strDrawLineStep;
		for (int ttx = x; ttx <= r; ++ttx)
		{
			for (int n = 0; n < dst.channels(); ++n)
			{
				double vtxt = subStr[n] / 255.0;
				int cvv = vtxt * color.val[n] + (1 - vtxt) * subImg[n];
				subImg[n] = cvv > 255 ? 255 : (cvv < 0 ? 0 : cvv);
			}
			subStr += 3;
			subImg += dst.channels();
		}
	}
	SelectObject(hDC, hOldBmp);
	SelectObject(hDC, hOldFont);
	DeleteObject(hf);
	DeleteObject(hBmp);
	DeleteObject(hDC);
}

double getZoom()//获取屏幕缩放比例
{
	HWND hWnd = GetDesktopWindow();
	HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

	// 获取监视器逻辑宽度
	MONITORINFOEX monitorInfo;
	monitorInfo.cbSize = sizeof(monitorInfo);
	GetMonitorInfo(hMonitor, &monitorInfo);
	int cxLogical = (monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left);

	// 获取监视器物理宽度
	DEVMODE dm;
	dm.dmSize = sizeof(dm);
	dm.dmDriverExtra = 0;
	EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &dm);
	int cxPhysical = dm.dmPelsWidth;

	return cxPhysical * 1.0 / cxLogical;
}

Mat screenshot(double zoom)//截图并返回结果
{
	HDC screenDC, compatibleDC;
	int width = GetSystemMetrics(SM_CXSCREEN) * zoom;
	int height = GetSystemMetrics(SM_CYSCREEN) * zoom;
	//获取屏幕长宽
	LPVOID screenshotdata = nullptr;
	HBITMAP hBitmap;

	screenshotdata = new char[width * height * 4];
	memset(screenshotdata, 0, width);

	// 获取屏幕 DC
	screenDC = GetDC(NULL);
	compatibleDC = CreateCompatibleDC(screenDC);

	// 创建位图
	hBitmap = CreateCompatibleBitmap(screenDC, width, height);
	SelectObject(compatibleDC, hBitmap);

	// 得到位图的数据
	BitBlt(compatibleDC, 0, 0, width, height, screenDC, 0, 0, SRCCOPY);
	GetBitmapBits(hBitmap, width * height * 4, screenshotdata);

	// 创建图像
	Mat screenshot(height, width, CV_8UC4, screenshotdata);

	ReleaseDC(NULL, screenDC);

	return screenshot;
}

double screenMatch(Mat temp,int &x,int &y)//完全匹配
{
	Mat img, img_;
	double zoom = getZoom();
	img_ = screenshot(zoom);

	cvtColor(img_, img, COLOR_RGBA2RGB);
	Mat result;
	matchTemplate(img, temp, result, TM_CCOEFF_NORMED);//模板匹配
	double maxVal, minVal;
	Point minLoc, maxLoc;
	//寻找匹配结果中的最大值和最小值以及坐标位置
	minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
	x = maxLoc.x;
	y = maxLoc.y;
	return maxVal;
}

void waitCtrlAlt()
{
	bool k1=false, k2 = false;
	if (Keydown(17))
	{
		k1 = true;
	}
	if (Keydown(18))
	{
		k2 = true;
	}
	DWORD stTime = timeGetTime();
	while (1)
	{
		if (k1 && (!Keydown(17)))
		{
			k1 = false;
		}
		if (k2 && (!Keydown(18)))
		{
			k2 = false;
		}
		if ((!(k1 || k2)) && Keydown(17) && Keydown(18))
		{
			if (timeGetTime() - stTime == 0)continue;
			break;
		}
		if (Pressbreak())
		{
			popError(9);
		}
	}	
	while (1)
	{	
		if ((!Keydown(17)) && (!Keydown(18)))
		{
			if (timeGetTime() - stTime == 0)continue;
			break;
		}
		if (Pressbreak())
		{
			popError(9);
		}
	}
}

void waitWithTime(int t)
{
	DWORD stTime = timeGetTime();
	while (1)
	{
		DWORD nowTime = timeGetTime();
		if (nowTime - stTime >= t)
		{
			break;
		}
		if (Pressbreak())
		{
			popError(9);
		}
	}
}

void makeRecord(string filename)
{
	int x, y,lsTime;
	double zoom = getZoom();
	ofstream fl(filename.c_str());
	cout << "按下 ctrl + alt 开始录制操作" << endl;
	waitCtrlAlt();
	cout << "录制开始, 按下 ctrl + shift + alt 结束录制操作" << endl;
	DWORD stTime = timeGetTime();
	int n = 0;
	POINT pt;
	while (1)
	{
		DWORD nowTime = timeGetTime();
		for (int i = 0;i<keyNum;i++)
		{
			if (Keydown(keyList[i]))
			{
				if (!bk[keyList[i]])
				{
					x = 0;
					y = 0;
					if (nowTime - stTime == 0)continue;
					bk[keyList[i]] = true; 
					if (keyList[i] == 1 || keyList[i] == 2 || keyList[i] == 4)
					{
						GetCursorPos(&pt);
						x = pt.x * zoom; 
						y = pt.y * zoom;
					}
					buffload(nowTime - stTime, keyList[i], (bk[keyList[i]] ? 1 : 0), x, y);
					n++;
				}
			}
			else
			{
				if (bk[keyList[i]])
				{
					x = 0;
					y = 0;
					if (nowTime - stTime == 0)continue;
					bk[keyList[i]] = false;
					if (keyList[i] == 1 || keyList[i] == 2 || keyList[i] == 4)
					{
						GetCursorPos(&pt);
						x = pt.x * zoom;
						y = pt.y * zoom;
					}
					buffload(nowTime - stTime, keyList[i], (bk[keyList[i]] ? 1 : 0), x, y);
					n++;
				}
			}
		}
		if (Keydown(17) && Keydown(18)&&Keydown(16))
		{
			if (nowTime - stTime == 0)continue;
			break;
		}
	}
	cout << "录制结束,共"<<n<<"次记录" << endl;
	lsTime = 0;
	for (int i = 0; i < n; i++)
	{
		if (i > 0)fl << endl;
		fl << "wait " << buff[0][i] - lsTime<<endl;
		lsTime = buff[0][i];
		if (buff[1][i] == 1 || buff[1][i] == 2 || buff[1][i] == 4)//鼠标操作
		{
			fl << "mousemove " << buff[3][i] << " " << buff[4][i] << endl;
			if (i < n - 1 && buff[1][i + 1] == buff[1][i] 
				&& buff[0][i + 1] - buff[0][i] <= 80
				&&buff[2][i]==1&&buff[2][i+1]==0)//按下即松开
			{
				fl << "click " << keyName(buff[1][i]);
				i++;
			}
			else
			{
				if (buff[2][i] == 1)
				{
					fl << "mousedown " << keyName(buff[1][i]);
				}
				else
				{
					fl << "mouseup " << keyName(buff[1][i]);
				}
			}
		}
		else
		{
			if (i < n - 1 && buff[1][i + 1] == buff[1][i]
				&& buff[0][i + 1] - buff[0][i] <= 80
				&& buff[2][i] == 1 && buff[2][i + 1] == 0)//按下即松开
			{
				fl << "press " << keyName(buff[1][i]);
				i++;
			}
			else
			{
				if (buff[2][i] == 1)
				{
					fl << "keydown " << keyName(buff[1][i]);
				}
				else
				{
					fl << "keyup " << keyName(buff[1][i]);
				}
			}
		}
	}
	cout << "已保存到" << filename << endl;
}

void playRecord(string filename)
{
	int time, key, curx, cury;
	string opt,namestr,line;
	double zoom = getZoom();

	int width = GetSystemMetrics(SM_CXSCREEN) * zoom;
	int height = GetSystemMetrics(SM_CYSCREEN) * zoom;

	ifstream fl(filename);
	if (!fl.is_open())
	{
		cout << "错误:文件打开失败" << endl;
		return;
	}
	int n = 0,now=0;
	while (getline(fl,line))
	{
		istringstream rd(line);
		curx = 0;
		cury = 0;
		rd >> opt; 
		if (rd.fail())//空行
		{
			n++;
			continue;
		}
		if (opt == "press")
		{
			rd >> namestr;
			key = keyCode(namestr);
			if (key == -1 || rd.fail())
			{
				popError(4,n+1);
			}
			buffload(1, key);
		}
		else if (opt == "keydown")
		{
			rd >> namestr;
			key = keyCode(namestr);
			if (key == -1 || rd.fail())
			{
				popError(4,n+1);
			}
			buffload(2, key);

		}
		else if (opt == "keyup")
		{
			rd >> namestr;
			key = keyCode(namestr);
			if (key == -1 || rd.fail())
			{
				popError(4,n+1);
			}
			buffload(3, key);

		}
		else if (opt == "click")
		{
			rd >> namestr;
			key = -1;
			if (namestr.length() == 6 && (namestr.substr(0, 5) == "mouse"))
			{
				char ch = namestr[5];
				if (ch == 'l' || ch == 'L')
				{
					key = 1;
				}
				else if (ch == 'r' || ch == 'R')
				{
					key = 2;
				}
				else if (ch == 'm' || ch == 'M')
				{
					key = 4;
				}
			}
			if (key == -1 || rd.fail())
			{
				popError(4,n+1);
			}
			buffload(4, key);
		}
		else if (opt == "mouseup")
		{
			rd >> namestr;
			key = -1;
			if (namestr.length() == 6 && (namestr.substr(0, 5) == "mouse"))
			{
				char ch = namestr[5];
				if (ch == 'l' || ch == 'L')
				{
					key = 1;
				}
				else if (ch == 'r' || ch == 'R')
				{
					key = 2;
				}
				else if (ch == 'm' || ch == 'M')
				{
					key = 4;
				}
			}
			if (key == -1 || rd.fail())
			{
				popError(4,n+1);
			}
			buffload(5, key);
		}
		else if (opt == "mousedown")
		{
			rd >> namestr;
			key = -1;
			if (namestr.length() == 6 && (namestr.substr(0, 5) == "mouse"))
			{
				char ch = namestr[5];
				if (ch == 'l' || ch == 'L')
				{
					key = 1;
				}
				else if (ch == 'r' || ch == 'R')
				{
					key = 2;
				}
				else if (ch == 'm' || ch == 'M')
				{
					key = 4;
				}
			}
			if (key == -1 || rd.fail())
			{
				popError(4,n+1);
			}
			buffload(6, key);
		}
		else if (opt == "mousemove")
		{
			rd >> curx >> cury;
			if (rd.fail())
			{
				popError(4,n+1);
			}
			buffload(7, curx,cury);
		}
		else if(opt == "wait")
		{
			rd >> time;
			if (rd.fail())
			{
				popError(4,n+1);
			}
			buffload(8, time);
		}
		else if(opt == "rest")
		{
			buffload(9);
		}
		else if (opt == "waitkey")
		{
			rd >> namestr;
			key = keyCode(namestr);
			if (key == -1 || rd.fail())
			{
				popError(4,n+1);
			}
			buffload(10, key);
			
		}
		else if (opt == "rwait")
		{
			rd >> time >> key;
			if (rd.fail())
			{
				popError(4, n + 1);
			}
			buffload(11, time, key);
		}
		else if (opt == "waitscreenscan")
		{
			rd >> namestr;
			if (rd.fail())
			{
				popError(4, n + 1);
			}
			buffload(12, room.size());
			room.push_back(namestr);
		}
		else if (opt == "loop")
		{
			rd >> time;
			if (time <= 0 || rd.fail())
			{
				popError(4, n + 1);
			}
			loopmark.push(n);
			buffload(13,time);
		}
		else if (opt == "endloop")
		{
			if (loopmark.empty())
			{
				popError(7, n + 1);
			}
			buffload(14);
			loopmark.pop();
		}
		else if (opt == "echo")
		{
			rd >> line;
			getline(rd, namestr);
			line += namestr;
			buffload(15, room.size());
			room.push_back(line);
		}
		else if (opt == "note")
		{
			buffload(16);
		}
		else
		{
			popError(5, n + 1);
		}
		n++;
	}
	if (!loopmark.empty())
	{
		popError(8, loopmark.top()+1);
	}
	cout << "按下 ctrl + alt 开始回放操作" << endl;
	waitCtrlAlt();
	DWORD stTime = timeGetTime();
	cout << "回放开始, 按下 ctrl + shift + alt 强制结束回放操作" << endl;
	Pressbreak();
	while (1)
	{
		DWORD nowTime = timeGetTime();
		if (now == n)
		{
			break;
		}
		if (buff[0][now] == 1)
		{
			keybd_event(buff[1][now], oemMark[buff[1][now]], 0, 0);
			Sleep(10);
			keybd_event(buff[1][now], oemBreak[buff[1][now]], KEYEVENTF_KEYUP, 0);
		}
		else if (buff[0][now] == 2)
		{
			keybd_event(buff[1][now], oemMark[buff[1][now]], 0, 0);
		}
		else if (buff[0][now] == 3)
		{
			keybd_event(buff[1][now], oemBreak[buff[1][now]], KEYEVENTF_KEYUP, 0);
		}
		else if (buff[0][now] == 4)
		{
			if (buff[1][now] == 1)//左键
			{
				mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
				Sleep(10);
				mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
			}
			else if (buff[1][now] == 2)//右键
			{
				mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
				Sleep(10);
				mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
			}
			else if (buff[1][now] == 4)//滚轮
			{
				mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, 0);
				Sleep(10);
				mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, 0);
			}
		}
		else if (buff[0][now] == 5)
		{
			if (buff[1][now] == 1)//左键
			{
				mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
			}
			else if (buff[1][now] == 2)//右键
			{
				mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
			}
			else if (buff[1][now] == 4)//滚轮
			{
				mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, 0);
			}
		}
		else if (buff[0][now] == 6)
		{
			if (buff[1][now] == 1)//左键
			{
				mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
			}
			else if (buff[1][now] == 2)//右键
			{
				mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
			}
			else if (buff[1][now] == 4)//滚轮
			{
				mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, 0);
			}
		}
		else if (buff[0][now] == 7)
		{
			mouse_event(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, buff[1][now] * 65536 / width, buff[2][now] * 65536 / height, 0, 0);
		}
		else if (buff[0][now] == 8)
		{
			waitWithTime(buff[1][now]);
		}
		else if (buff[0][now] == 9)
		{
			cout << "按下 ctrl + alt 继续回放操作" << endl;
			waitCtrlAlt();
		}
		else if (buff[0][now] == 10)
		{
			key = buff[1][now];
			cout << "按下 "<<keyName(buff[1][now])<<" 继续回放操作" << endl;
			bool k1 = false;
			if (Keydown(key))
			{
				k1 = true;
			}
			DWORD stTime = timeGetTime();
			while (1)
			{
				if (k1 && (!Keydown(17)))
				{
					k1 = false;
				}
				if ((!k1) && Keydown(key))
				{
					if (timeGetTime() - stTime == 0)continue;
					break;
				}
				if (Pressbreak())
				{
					popError(9);
				}
			}
			while (1)
			{
				if ((!Keydown(key)))
				{
					if (timeGetTime() - stTime == 0)continue;
					break;
				}
				if (Pressbreak())
				{
					popError(9);
				}
			}
		}
		else if (buff[0][now] == 11)
		{
			int t = buff[1][now] + (((rand() << 15) + rand()) % (buff[2][now]+1));
			waitWithTime(t);
		}
		else if (buff[0][now] == 12)
		{
			namestr = room[buff[1][now]];
			struct stat s;
			if (stat(namestr.c_str(), &s) != 0)
			{
				popError(6, buff[1][now]);
			}
			Mat temp;
			temp = imread(namestr.c_str());
			double cl = 0;
			while (cl < PICACC)
			{
				waitWithTime(SCANGAP);
				cl=screenMatch(temp,curx,cury);
			}
		}
		else if (buff[0][now] == 13)
		{
			loopmark.push(now);
			loopcnt.push(0);
		}
		else if (buff[0][now] == 14)
		{
			int t = loopcnt.top();
			loopcnt.pop();
			int lp = loopmark.top();
			if (t + 1 < buff[1][lp])
			{
				now = lp;
				loopcnt.push(t + 1);
			}
			else
			{
				loopmark.pop();
			}
		}
		else if (buff[0][now] == 15)
		{
			cout << room[buff[1][now]] << endl;
		}
		else if (buff[0][now] == 16)
		{
			//do nothing
			//因为这是注释占位
		}
		now++;
	}
	cout << "回放已结束" << endl;
}

int main(int argc,char **argv)
{
	bool flg = true;
	srand(time(NULL));
	makeKeyList();
	makeOem();
	memset(bk, false, sizeof(bk));
	string name;
	if (argc == 1)
	{
		while (flg)
		{
			cout << "请输入需要保存的脚本名称:" << endl;
			cin >> name;
			getchar();
			struct stat s;
			if (name.substr(name.size() - 3, 3) != ".sr")
			{
				name.append(".sr");
			}
			flg = false;
			if (stat(name.c_str(), &s) == 0)
			{
				cout << "文件已存在,是否覆盖?(Y/N)";
				char ch = getchar();
				if (ch != 'Y')
				{
					flg = true;
				}
			}
			if (flg == false)
			{
				makeRecord(name);
			}
		}
		system("pause");
	}
	else if(argc==2)
	{
		name = argv[1];
		struct stat s;
		if (stat(name.c_str(), &s) != 0)
		{
			popError(1);//文件不存在
		}
		else
		{
			if (s.st_mode & S_IFREG)
			{
				if (name.substr(name.size() - 3, 3) != ".sr")
				{
					popError(2);//文件类型不匹配
				}
				else
				{
					playRecord(name);
				}
			}
			else
			{
				popError(3);
			}
		}
		system("pause");
	}
	return 0;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
