/************************************************************************/
/* 
Description:	基本的粒子滤波目标跟踪
Author:			Yang Xian
Email:			yang_xian521@163.com
Version:		2011-11-2
History:
*/
/************************************************************************/
#include <iostream>	// for standard I/O
#include <string>   // for strings
#include <iomanip>  // for controlling float print precision
#include <sstream>  // string to number conversion

#include <opencv2/imgproc/imgproc.hpp>  
#include <opencv2/core/core.hpp>        // Basic OpenCV structures (cv::Mat, Scalar)
#include <opencv2/highgui/highgui.hpp>  // OpenCV window I/O

using namespace cv;
using namespace std;

// 以下这些参数对结果影响很大，而且也会根据视频内容，会对结果有很大的影响
const int PARTICLE_NUM = 25;	// 粒子个数
// 粒子放入的相关区域
const double A1 = 2.0;
const double A2 = -1.0;
const double B0 = 1.0;
// 高斯随机数sigma参数
const double SIGMA_X = 1.0;
const double SIGMA_Y = 0.5;
const double SIGMA_SCALE = 0.001;

// 粒子结构体
typedef struct particle {
	double x;			// 当前x坐标
	double y;			// 当前y坐标
	double scale;		// 窗口比例系数
	double xPre;			// x坐标预测位置
	double yPre;			// y坐标预测位置
	double scalePre;		// 窗口预测比例系数
	double xOri;			// 原始x坐标
	double yOri;			// 原始y坐标
// 	int width;			// 原始区域宽度
// 	int height;			// 原始区域高度
	Rect rect;			// 原始区域大小
	MatND hist;			// 粒子区域的特征直方图
	double weight;		// 该粒子的权重
} PARTICLE;

Mat hsv;	// hsv色彩空间的输入图像
Mat roiImage;	// 目标区域
MatND roiHist;	// 目标区域直方图
Mat img;	// 输出的目标图像
PARTICLE particles[PARTICLE_NUM];	// 粒子

int nFrameNum = 0;

bool bSelectObject = false;	// 区域选择标志
bool bTracking = false;		// 开始跟踪标志
Point origin;	// 鼠标按下时的点位置
Rect selection;// 感兴趣的区域大小

// 直方图相关参数，特征的选取也会对结果影响巨大
// Quantize the hue to 30 levels色调
// and the saturation to 32 levels 饱和度
// value to 10 levels亮度值
int hbins = 180, sbins = 256, vbin = 10;
int histSize[] = {hbins, sbins, vbin};
// hue varies from 0 to 179, see cvtColor
float hranges[] = { 0, 180 };
// saturation varies from 0 (black-gray-white) to 255 (pure spectrum color)
float sranges[] = { 0, 256 };
// value varies from 0 (black-gray-white) to 255 (pure spectrum color)
float vranges[] = { 0, 256 };
const float* ranges[] = {hranges, sranges, vranges};
// we compute the histogram from the 0-th and 1-st channels
int channels[] = {0, 1, 2};

// 鼠标响应函数，得到选择的区域，保存在selection
void onMouse(int event, int x, int y, int, void*)
{
	if( bSelectObject )
	{
		selection.x = MIN(x, origin.x);
		selection.y = MIN(y, origin.y);
		selection.width = std::abs(x - origin.x);
		selection.height = std::abs(y - origin.y);

		selection &= Rect(0, 0, img.cols, img.rows);
	}

	switch (event)
	{
	case CV_EVENT_LBUTTONDOWN:
		origin = Point(x,y);
		selection = Rect(x,y,0,0);
		bSelectObject = true;
		bTracking = false;
		break;
	case CV_EVENT_LBUTTONUP:
		bSelectObject = false;
		bTracking = true;
		nFrameNum = 0;
		break;
	}
}

// 快速排序算法排序函数
int particle_cmp(const void* p1,const void* p2)
{
	PARTICLE* _p1 = (PARTICLE*)p1;
	PARTICLE* _p2 = (PARTICLE*)p2;

	if(_p1->weight < _p2->weight)
		return 1;	//按照权重降序排序
	if(_p1->weight > _p2->weight)
		return -1;
	return 0;
}

int main(int argc, char *argv[])
{
	int delay = 10;	// 控制播放速度
	char c;	// 键值

	VideoCapture captRefrnc("hockey.avi"/*"soccer.avi"*/);	// 视频文件

	if ( !captRefrnc.isOpened())
	{
		return -1;
	}

	// Windows
// 	const char* WIN_SRC = "Source";
	const char* WIN_RESULT = "Result";
// 	namedWindow(WIN_SRC, CV_WINDOW_AUTOSIZE );
	namedWindow(WIN_RESULT, CV_WINDOW_AUTOSIZE);
	// 鼠标响应函数
	setMouseCallback(WIN_RESULT, onMouse, 0);

	Mat frame;	//视频的每一帧图像

	bool paused = false;
 	PARTICLE * pParticles = particles;
//	PARTICLE * pParticles = new PARTICLE[sizeof(PARTICLE) * PARTICLE_NUM];

	while(true) //Show the image captured in the window and repeat
	{
		if(!paused)
		{
			captRefrnc >> frame;
			if(frame.empty())
				break;
		}

		frame.copyTo(img);	// 接下来的操作都是对src的

		// 选择目标后进行跟踪
		if (bTracking == true)
		{
			if(!paused)
			{				
				nFrameNum++;
				cvtColor(img, hsv, CV_BGR2HSV);
				Mat roiImage(hsv, selection);	// 目标区域

				if (nFrameNum == 1)	//选择目标后的第一帧需要初始化
				{
					// step 1: 提取目标区域特征
					calcHist(&roiImage, 1, channels, Mat(), roiHist, 3, histSize, ranges);
					normalize(roiHist, roiHist);	// 归一化L2

					// step 2: 初始化particle
					pParticles = particles;
					for (int i=0; i<PARTICLE_NUM; i++)
					{
						pParticles->x = selection.x + 0.5 * selection.width;
						pParticles->y = selection.y + 0.5 * selection.height;
						pParticles->xPre = pParticles->x;
						pParticles->yPre = pParticles->y;
						pParticles->xOri = pParticles->x;
						pParticles->yOri = pParticles->y;
						pParticles->rect = selection;
						pParticles->scale = 1.0;
						pParticles->scalePre = 1.0;
						pParticles->hist = roiHist;
						pParticles->weight = 0;
						pParticles++;
					}
				}
				else
				{					
					pParticles = particles;	
					RNG rng;
					for (int i=0; i<PARTICLE_NUM; i++)
					{
						// step 3: 求particle的transition
						double x, y, s;

						pParticles->xPre = pParticles->x;
						pParticles->yPre = pParticles->y;
						pParticles->scalePre = pParticles->scale;

						x = A1 * (pParticles->x - pParticles->xOri) + A2 * (pParticles->xPre - pParticles->xOri) +
							B0 * rng.gaussian(SIGMA_X) + pParticles->xOri;
						pParticles->x = std::max(0.0, std::min(x, img.cols-1.0));
						

						y = A1 * (pParticles->y - pParticles->yOri) + A2 * (pParticles->yPre - pParticles->yOri) +
							B0 * rng.gaussian(SIGMA_Y) + pParticles->yOri;
						pParticles->y = std::max(0.0, std::min(y, img.rows-1.0));

						s = A1 * (pParticles->scale - 1.0) + A2 * (pParticles->scalePre - 1.0) +
							B0 * rng.gaussian(SIGMA_SCALE) + 1.0;
						pParticles->scale = std::max(0.1, std::min(s, 3.0));
						
						// rect参数有待考证
						pParticles->rect.x = std::max(0, std::min(cvRound(pParticles->x - 0.5 * pParticles->rect.width * pParticles->scale), img.cols-1));		// 0 <= x <= img.rows-1
						pParticles->rect.y = std::max(0, std::min(cvRound(pParticles->y - 0.5 * pParticles->rect.height * pParticles->scale), img.rows-1));	// 0 <= y <= img.cols-1
						pParticles->rect.width = std::min(cvRound(pParticles->rect.width * pParticles->scale), img.cols - pParticles->rect.x);
						pParticles->rect.height = std::min(cvRound(pParticles->rect.height * pParticles->scale), img.rows - pParticles->rect.y);
						// Ori参数不改变
						
						// step 4: 求particle区域的特征直方图
						Mat imgParticle(img, pParticles->rect);
						calcHist(&imgParticle, 1, channels, Mat(), pParticles->hist, 3, histSize, ranges);
						normalize(pParticles->hist, pParticles->hist);	// 归一化L2

						// step 5: 特征的比对,更新particle权重
						pParticles->weight = compareHist(roiHist, pParticles->hist, CV_COMP_INTERSECT);

						pParticles++;	
					}

					// step 6: 归一化粒子权重
					double sum = 0.0;
					int i;

					pParticles = particles;
					for(i=0; i<PARTICLE_NUM; i++)
					{
						sum += pParticles->weight;
						pParticles++;
					}
					pParticles = particles;
					for(i=0; i<PARTICLE_NUM; i++)
					{
						pParticles->weight /= sum;
						pParticles++;
					}

					// step 7: resample根据粒子的权重的后验概率分布重新采样
					pParticles = particles;
// 					PARTICLE* newParticles = new PARTICLE[sizeof(PARTICLE) * PARTICLE_NUM];
					PARTICLE newParticles[PARTICLE_NUM];
					int np, k = 0;

					qsort(pParticles, PARTICLE_NUM, sizeof(PARTICLE), &particle_cmp);
					for(int i=0; i<PARTICLE_NUM; i++)
					{
						np = cvRound(particles[i].weight * PARTICLE_NUM);   //后验概率 = 粒子权重*总粒子数
						for(int j=0; j<np; j++)
						{
							newParticles[k++] = particles[i];
							if(k == PARTICLE_NUM)
								goto EXITOUT;
						}
					}
					while(k < PARTICLE_NUM)
					{
						newParticles[k++] = particles[0];
					}

				EXITOUT:
					for (int i=0; i<PARTICLE_NUM; i++)
					{
						particles[i] = newParticles[i];
					}

				}// end else

				qsort(pParticles, PARTICLE_NUM, sizeof(PARTICLE), &particle_cmp);

				// step 8: 计算粒子的期望，作为跟踪结果
				Rect_<double> rectTrackingTemp(0.0, 0.0, 0.0, 0.0);
				pParticles = particles;
				for (int i=0; i<PARTICLE_NUM; i++)
				{
					rectTrackingTemp.x += pParticles->rect.x * pParticles->weight;
					rectTrackingTemp.y += pParticles->rect.y * pParticles->weight;
					rectTrackingTemp.width += pParticles->rect.width * pParticles->weight;
					rectTrackingTemp.height += pParticles->rect.height * pParticles->weight;
					pParticles++;
				}
				Rect rectTracking(rectTrackingTemp);	// 跟踪结果

				// 显示各粒子的运动
				for (int i=0; i<PARTICLE_NUM; i++)
				{
					rectangle(img, particles[i].rect, Scalar(255,0,0));
				}
				// 显示跟踪结果
 				rectangle(img, rectTracking, Scalar(0,0,255), 3);

			}
		}// end Tracking

// 		imshow(WIN_SRC, frame);
		imshow(WIN_RESULT, img);

		c = (char)waitKey(delay);
		if( c == 27 )
			break;
		switch(c)
		{
		case 'p':
			paused = !paused;
			break;
		default:
			;
		}
	}// end while
}