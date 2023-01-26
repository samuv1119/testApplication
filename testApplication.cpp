// testApplication.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

//todo.设计成初步的脚本语言

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
#pragma comment(lib,"winmm.lib")

#define Keydown(key) ((GetAsyncKeyState(key) && 0x8000)?1:0)

using namespace cv;
using namespace std;

vector<int> buff[5];//time key opt <curx> <cury>
bool bk[500];
int keyList[500],keyNum=0;
int oemMark[500],oemBreak[500];

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

double screenMatch(string filename,int &x,int &y)//完全匹配
{
	Mat img, temp,img_;
	temp=imread(filename.c_str());
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

void bStart()
{
	DWORD stTime = timeGetTime();
	while (1)
	{
		if (Keydown(17) && Keydown(18))
		{
			if (timeGetTime() - stTime == 0)continue;
			break;
		}
	}
}

void makeRecord(string filename)
{

	double zoom = getZoom();
	ofstream fl(filename.c_str());
	cout << "按下 ctrl + alt 开始录制操作" << endl;
	bStart();
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
					if (nowTime - stTime == 0)continue;
					bk[keyList[i]] = true;
					if (n)
					{
						fl << endl;
					}
					fl << nowTime - stTime << " " << keyList[i] << " " << (bk[keyList[i]] ? 1 : 0);
					if (keyList[i] == 1 || keyList[i] == 2 || keyList[i] == 4)
					{
						GetCursorPos(&pt);
						fl << " " << int(pt.x*zoom) << " " << int(pt.y*zoom);
					}
					n++;
				}
			}
			else
			{
				if (bk[keyList[i]])
				{
					if (nowTime - stTime == 0)continue;
					bk[keyList[i]] = false;
					if (n)
					{
						fl << endl;
					}
					fl << nowTime - stTime << " " << keyList[i] << " " << (bk[keyList[i]] ? 1 : 0);
					if (keyList[i] == 1 || keyList[i] == 2 || keyList[i] == 4)
					{
						GetCursorPos(&pt);
						fl << " " << int(pt.x * zoom) << " " << int(pt.y * zoom);
					}
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
	cout << "录制结束,共"<<n<<"次记录,结果已保存到" <<filename<< endl;
}

void playRecord(string filename)
{
	int time, key, opt, curx, cury;
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
	bool fKey = true;
	while (!fl.eof())
	{
		curx = 0;
		cury = 0;
		fl >> time >> key >> opt;
		if (key == 1 || key == 2 || key == 4)
		{
			fl >> curx >> cury;
		}
		buff[0].push_back(time);
		buff[1].push_back(key);
		buff[2].push_back(opt);
		buff[3].push_back(curx);
		buff[4].push_back(cury);
		n++;
	}
	cout << "是否使用开头按键激活回放(Y/N)" << endl;
	char ch = getchar();
	if (ch != 'Y')
	{
		fKey=false;
	}
	cout << "按下 ctrl + alt 开始回放操作" << endl;
	bStart();
	DWORD stTime = timeGetTime();
	if (fKey)
	{
		while (1)
		{
			DWORD nowTime = timeGetTime();
			if (Keydown(buff[1][now]))
			{
				if (nowTime - stTime == 0)continue;
				stTime = nowTime - buff[0][now];
				break;
			}
		}
		now++;
	}
	cout << "回放开始, 按下 ctrl + shift + alt 强制结束回放操作" << endl;
	while (1)
	{
		DWORD nowTime = timeGetTime();
		if (now == n)
		{
			break;
		}
		if (Keydown(17) && Keydown(18) && Keydown(16))
		{
			if (nowTime - stTime == 0)continue;
			break;
		}
		if ((((long long)nowTime - stTime) - buff[0][now] < 20) && (((long long)nowTime - stTime) - buff[0][now] > -20))
		{

			if (key == 1 || key == 2 || key == 4)//鼠标操作
			{
				if (buff[2][now] == 1)
				{
					if (key == 1)//左键
					{
						mouse_event(MOUSEEVENTF_ABSOLUTE| MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_MOVE, buff[3][now]*65536/width, buff[4][now]*65536/height, 0, 0);
					}
					else if (key == 2)//右键
					{
						mouse_event(MOUSEEVENTF_ABSOLUTE| MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_MOVE, buff[3][now]*65536/width, buff[4][now]*65536/height, 0, 0);
					}
					else if (key == 4)//滚轮
					{
						mouse_event(MOUSEEVENTF_ABSOLUTE| MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_MOVE, buff[3][now]*65536/width, buff[4][now]*65536/height, 0, 0);
					}
					cout << "按下" << buff[1][now] << endl;
				}
				else
				{
					if (key == 1)//左键
					{
						mouse_event(MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_LEFTUP | MOUSEEVENTF_MOVE, buff[3][now]*65536/width, buff[4][now]*65536/height, 0, 0);
					}
					else if (key == 2)//右键
					{
						mouse_event(MOUSEEVENTF_ABSOLUTE| MOUSEEVENTF_RIGHTUP | MOUSEEVENTF_MOVE, buff[3][now]*65536/width, buff[4][now]*65536/height, 0, 0);
					}
					else if (key == 4)//滚轮
					{
						mouse_event(MOUSEEVENTF_ABSOLUTE| MOUSEEVENTF_MIDDLEUP | MOUSEEVENTF_MOVE, buff[3][now]*65536/width, buff[4][now]*65536/height, 0, 0);
					}
				}
				cout << "弹起" << buff[1][now] << endl;
			}
			else
			{
				if (buff[2][now] == 1)
				{
					keybd_event(buff[1][now], oemMark[buff[1][now]], 0, 0);
					cout << "按下" << buff[1][now] << endl;
				}
				else
				{
					keybd_event(buff[1][now], oemBreak[buff[1][now]], KEYEVENTF_KEYUP, 0);
					cout << "弹起" << buff[1][now] << endl;
				}
			}
			now++;
		}
	}
	cout << "回放已结束" << endl;
}

int main(int argc,char **argv)
{
	makeKeyList();
	makeOem();
	memset(bk, false, sizeof(bk));
	string name;
	if (argc == 1)
	{
		cout << "请输入需要保存的脚本名称:" << endl;
		cin >> name;
		getchar();
		struct stat s;
		if (name.substr(name.size() - 3, 3) != ".sr")
		{
			name.append(".sr");
		}
		if (stat(name.c_str(), &s)==0)
		{
			cout << "文件已存在,是否覆盖?(Y/N)";
			char ch = getchar();
			if (ch != 'Y')
			{
				return 0;
			}
		}
		if (Keydown('A'))
		{
			cout << "mark"<<endl;
		}
		makeRecord(name);
		system("pause");
	}
	else if(argc==2)
	{
		name = argv[1];
		struct stat s;
		if (stat(name.c_str(), &s) != 0)
		{
			cout << "错误:文件不存在" << endl;
		}
		else
		{
			if (s.st_mode & S_IFREG)
			{
				if (name.substr(name.size() - 3, 3) != ".sr")
				{
					cout << "错误:文件类型不匹配" << endl;
				}
				else
				{
					playRecord(name);
				}
			}
			else
			{
				cout << "错误:打开的不是一个文件" << endl;
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
