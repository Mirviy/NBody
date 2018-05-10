#pragma comment(lib,"Imm32.lib")
#pragma comment(lib,"glu32.lib")
#pragma comment(lib,"opengl32.lib")
#include<windows.h>
#include<gl/gl.h>
#include<gl/glu.h>
#include<cl/opencl.h>
#include<cmath>
#include<stdint.h>
#include<stdio.h>
#include"Vector.h"
#include"clutils.h"
#include"input.h"
#include"random32.h"

struct {
	HDC hdc;
	HGLRC hglrc;
	GLdouble disgoal,dis,Azimuth,Altitude;
	Coordinate look;
	Vector loc;
	GLint width,height;
	POINTS mouseopos;
	BOOL dragging,moved;
	Coordinate coor;
	int disstep;
} Sky;

float deltaTime=0.003f;
float damping=0.006f;
float softSq=1;
int workdim;
cl_int num=2048*6;
cl_mem position[2];//{old,new}
cl_context clctx=0;
cl_command_queue clcq=0;
cl_kernel clkernel=0;
GLuint texStar;
float (*pos)[8];//{x,y,z,m,vx,vy,vz,m}
double (*color)[3];//{R,G,B}

void HSVtoRGB(double H,double S,double V,double &R,double &G,double &B){
	H*=6;
	double C=S*V,X=C*(1-abs(H-2*floor(H/2)-1)),M=V-C;
	C+=M;
	X+=M;
		 if(H<1)R=C,G=X,B=M;
	else if(H<2)R=X,G=C,B=M;
	else if(H<3)R=M,G=C,B=X;
	else if(H<4)R=M,G=X,B=C;
	else if(H<5)R=X,G=M,B=C;
	else		R=C,G=M,B=X;
}
BOOL MySetPixelFormat(HDC hdc){
	PIXELFORMATDESCRIPTOR pfd={
		sizeof(PIXELFORMATDESCRIPTOR),		//pfd结构的大小
		1,									//版本号
		PFD_DRAW_TO_WINDOW|					//支持在窗口中绘图
		PFD_SUPPORT_OPENGL|					//支持OpenGL
		PFD_DOUBLEBUFFER,					//双缓存模式
		PFD_TYPE_RGBA,						//RGBA颜色模式
		32,									//32位颜色深度
		8,0,8,0,8,0,8,0,					//RGBA分量各8位,忽略移位位
		0,									//无累计缓存
		0,0,0,0,							//忽略累计位
		32,									//32位深度缓存
		0,									//无模板缓存
		0,									//无辅助缓存
		PFD_MAIN_PLANE,						//主层
		0,									//保留
		0,0,0								//忽略层,可见性和损毁掩模
	};

	int	iPixelFormat;
	//为设备描述表得到最匹配的像素格式
	if((iPixelFormat=ChoosePixelFormat(hdc,&pfd))==0)return 0;
	//设置最匹配的像素格式为当前的像素格式
	if(!SetPixelFormat(hdc,iPixelFormat,&pfd))return 0;
	return 1;
}

GLuint LoadGLTexture(const LPTSTR szFileName,GLdouble *avrm=0,GLdouble *avgm=0,GLdouble *avbm=0){
	HBITMAP hBMP;
	BITMAP BMP;
	GLuint texName;
	glGenTextures(1,&texName);
	if(texName==0)return 0;
	hBMP=(HBITMAP)LoadImage(GetModuleHandle(NULL),szFileName,IMAGE_BITMAP,0,0,LR_CREATEDIBSECTION|LR_LOADFROMFILE);
	if(!hBMP)return 0;
	GetObject(hBMP,sizeof(BMP),&BMP);
	glPixelStorei(GL_UNPACK_ALIGNMENT,4);
	glBindTexture(GL_TEXTURE_2D,texName);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D,0,3,BMP.bmWidth,BMP.bmHeight,0,GL_BGR_EXT,GL_UNSIGNED_BYTE,BMP.bmBits);
	if(avrm&&avgm&&avbm){
		int64_t R=0,G=0,B=0,maxRGB,ulim=(int64_t)BMP.bmWidth*BMP.bmHeight;
		BYTE *data=(BYTE*)BMP.bmBits;
		for(int64_t i=0;i<ulim;++i){
			B+=data[3*i];
			G+=data[3*i+1];
			R+=data[3*i+2];
		}
		maxRGB=R;
		if(G>maxRGB)maxRGB=G;
		if(B>maxRGB)maxRGB=B;
		if(maxRGB){
			*avrm=(GLdouble)R/maxRGB;
			*avgm=(GLdouble)G/maxRGB;
			*avbm=(GLdouble)B/maxRGB;
		}
	}
	DeleteObject(hBMP);
	return texName;
}

BOOL Initialize(HWND hWnd){
	//disable imm
	HIMC hImcId=::ImmGetContext(hWnd);
	if(hImcId){
		::ImmAssociateContext(hWnd,NULL);
		::ImmReleaseContext(hWnd,hImcId);
		::SetFocus(hWnd);
	}

	//init opencl
	cl_device_id cldev=GetDeviceID();
	if(!cldev)return 0;

	cl_int clerr;
	clctx=clCreateContext(0,1,&cldev,NULL,NULL,&clerr);
	if(clerr!=CL_SUCCESS){
		printf(" Error %i in clCreateContext Call !!!\n\n",clerr);
		return 0;
	}

	clcq=clCreateCommandQueue(clctx,cldev,NULL,&clerr);
	if(clerr!=CL_SUCCESS){
		printf(" Error %i in clCreateCommandQueue Call !!!\n\n",clerr);
		return 0;
	}

	workdim=CreateKernel("NbodyKernel.cl",clctx,cldev,"integrateBodies_noMT",&clkernel);
	if(!workdim)return 0;
	
	//init pos
	num=initsystem(workdim,deltaTime,softSq,damping,pos);
	if(!num)return 0;

	unsigned int memSize=sizeof(float)*8*num;
	position[0]=clCreateBuffer(clctx,CL_MEM_READ_WRITE,memSize,NULL,NULL);
	position[1]=clCreateBuffer(clctx,CL_MEM_READ_WRITE,memSize,NULL,NULL);
	
	color=new double[num][3];

	for(int i=0;i<num;++i){
		HSVtoRGB(random(),random(),1,color[i][0],color[i][1],color[i][2]);
	}

	clEnqueueWriteBuffer(clcq,position[0],CL_TRUE,0,memSize,pos,0,NULL,NULL);

	//init opengl
	ShowWindow(hWnd,SW_MAXIMIZE);

	Sky.disgoal=Sky.dis=60;
	Sky.Azimuth=Sky.Altitude=0;
	Sky.dragging=0;
	Sky.disstep=0;
	Sky.hdc=GetDC(hWnd);
	if(!MySetPixelFormat(Sky.hdc))return 0;
	Sky.hglrc=wglCreateContext(Sky.hdc);
	wglMakeCurrent(Sky.hdc,Sky.hglrc);
	glClearColor(0.f,0.f,0.f,0.f);
	glShadeModel(GL_SMOOTH);
	texStar=LoadGLTexture(TEXT("star"));

	SetTimer(hWnd,10086,25,NULL);
	UpdateWindow(hWnd);
	return 1;
}

void mglBeginPointQuads(){
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0,Sky.width,Sky.height,0,-1,1);
	glBegin(GL_QUADS);
}
void mglPointQuads(GLdouble winx,GLdouble winy,GLdouble size){
	size/=2;
	glTexCoord2d(0.,0.);
	glVertex2d(winx-size,winy-size);
	glTexCoord2d(1.,0.);
	glVertex2d(winx+size,winy-size);
	glTexCoord2d(1.,1.);
	glVertex2d(winx+size,winy+size);
	glTexCoord2d(0.,1.);
	glVertex2d(winx-size,winy+size);
}
void mglEndPointQuads(){
	glEnd();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}

void PaintGL(HDC hdc){
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60,(GLdouble)Sky.width/Sky.height,0.01,10000);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	Vector cen(Sky.coor,Sky.Azimuth,Sky.Altitude),up(Sky.coor,Sky.Azimuth,Sky.Altitude+pi/2);
	Sky.look=Coordinate(0,up,-cen);
	Sky.loc=Sky.look.z*Sky.dis;
	
	GLdouble modelview[16],projection[16];
	GLint viewport[4];
	glGetDoublev(GL_MODELVIEW_MATRIX,modelview);
	glGetDoublev(GL_PROJECTION_MATRIX,projection);
	glGetIntegerv(GL_VIEWPORT,viewport);

	//Stars
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE_MINUS_DST_ALPHA,GL_ONE);
	glBindTexture(GL_TEXTURE_2D,texStar);
	mglBeginPointQuads();

	for(int i=0;i<num;++i){
		Vector ir(pos[i][0],pos[i][1],pos[i][2]);
		Vector spos=ir-Sky.loc;
		if(spos%cen>0){
			GLdouble winx,winy,winz;
			Vector lookpos=Sky.look.trans(spos);
			gluProject(
				lookpos.x,lookpos.y,lookpos.z,
				modelview,projection,viewport,
				&winx,&winy,&winz);
			winy=Sky.height-winy;
			if(winx>=-100&&winx<=Sky.width+100&&winy>=-100&&winy<=Sky.height+100){
				double A=0.15+0.85*pow(pos[i][3]/(spos%spos+1),0.23);
				GLdouble nsize=30*A*A;
				glColor4d(color[i][0]*A,color[i][1]*A,color[i][2]*A,0.);
				mglPointQuads(winx,winy,nsize);
			}
		}
	}
	
	mglEndPointQuads();
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	
	SwapBuffers(hdc);
}

void TimerEvent(WPARAM wParam){

	//dis
	if(Sky.disstep){
		double t=--Sky.disstep;
		t=(t-15)*t*t/((t-14)*(t+1)*(t+1));
		Sky.dis=exp(log(Sky.disgoal)*(1-t)+t*log(Sky.dis));
	}
	
	//update
	int sharedMemSize=workdim*sizeof(cl_float8);// 8 doubles for pos
	cl_int clerr=CL_SUCCESS;
	
	clerr|=clSetKernelArg(clkernel,0,sizeof(cl_mem),position+1);//new
	clerr|=clSetKernelArg(clkernel,1,sizeof(cl_mem),position);//old
	clerr|=clSetKernelArg(clkernel,2,sizeof(cl_float),&deltaTime);
	clerr|=clSetKernelArg(clkernel,3,sizeof(cl_float),&damping);
	clerr|=clSetKernelArg(clkernel,4,sizeof(cl_float),&softSq);
	clerr|=clSetKernelArg(clkernel,5,sizeof(cl_int),&num);
	clerr|=clSetKernelArg(clkernel,6,sharedMemSize,NULL);
	if(clerr!=CL_SUCCESS){
		printf(" Error %i in clSetKernelArg Calls !!!\n\n",clerr);
		exit(1);
	}
	
	// set work-item dimensions
	size_t global_work_size[2]={num,1};
	size_t local_work_size[2]={workdim,1};
	
	// execute the clkernel:
	clerr=clEnqueueNDRangeKernel(clcq,clkernel,2,NULL,global_work_size,local_work_size,0,NULL,NULL);
	if(clerr!=CL_SUCCESS){
		printf(" Error %i in clEnqueueNDRangeKernel Call !!!\n\n",clerr);
		exit(1);
	}
	
	//swap new&old
	cl_mem tmpmem;
	tmpmem=position[0];
	position[0]=position[1];
	position[1]=tmpmem;
	
	clerr=clEnqueueReadBuffer(clcq,position[0],CL_TRUE,0,num*8*sizeof(float),pos,0,NULL,NULL);
	if(clerr!=CL_SUCCESS){
		exit(1);
	}
}

void KeyDownEvent(WPARAM wParam,bool shift){

}

LRESULT CALLBACK WndProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam){
	static bool shift=false;
	switch(message){
	case WM_TIMER:
		if(wParam==10086)TimerEvent(wParam);
		InvalidateRect(hWnd,NULL,FALSE);
		break;
	case WM_KEYDOWN:
		if(wParam==VK_SHIFT)shift=true;
		else KeyDownEvent(wParam,shift);
		break;
	case WM_KEYUP:
		if(wParam==VK_SHIFT)shift=false;
		break;
	case WM_LBUTTONDOWN:
		Sky.dragging=1;
		Sky.moved=0;
		Sky.mouseopos=MAKEPOINTS(lParam);
		break;
	case WM_LBUTTONUP:
		Sky.dragging=0;
		break;
	case WM_MOUSEMOVE:
		if(Sky.dragging){
			Sky.moved=1;
			POINTS point=MAKEPOINTS(lParam);
			Vector Bo(Sky.mouseopos.x-Sky.width/2,Sky.height/(2*tan(60/(2*rad))),Sky.height/2-Sky.mouseopos.y),
				Bn(point.x-Sky.width/2,Sky.height/(2*tan(60/(2*rad))),Sky.height/2-point.y);
	
			Sky.coor.rotz(getlon(Bn)-getlon(Bo));
			Sky.coor.roty(getlat(Bo)-getlat(Bn));

			Sky.mouseopos=point;
		}
		break;
	case WM_MOUSEWHEEL:
		{
			double ndisgoal=Sky.disgoal*pow(1.002,-GET_WHEEL_DELTA_WPARAM(wParam));
			if(ndisgoal<100&&ndisgoal>0.001){
				Sky.disgoal=ndisgoal;
				Sky.disstep=10;
			}
		}
		break;
	case WM_SIZE:
		glViewport(0,0,Sky.width=LOWORD(lParam),Sky.height=HIWORD(lParam));
		break;
	case WM_PAINT:
		BeginPaint(hWnd,NULL);
		if(Sky.width&&Sky.height)PaintGL(Sky.hdc);
		EndPaint(hWnd,NULL);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd,message,wParam,lParam);
		break;
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow){
	WNDCLASSEX wcex;
	HWND hWnd;
	MSG msg;
	TCHAR szWindowClass[]=TEXT("3DView");
	TCHAR szTitle[]=TEXT("3DView");

	wcex.cbSize=sizeof(WNDCLASSEX);
	wcex.style=0;
	wcex.lpfnWndProc=WndProc;
	wcex.cbClsExtra=0;
	wcex.cbWndExtra=0;
	wcex.hInstance=hInstance;
	wcex.hIcon=LoadIcon(hInstance,IDI_APPLICATION);
	wcex.hCursor=LoadCursor(NULL,IDC_ARROW);
	wcex.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
	wcex.lpszMenuName=NULL;
	wcex.lpszClassName=szWindowClass;
	wcex.hIconSm=LoadIcon(hInstance,IDI_APPLICATION);

	if(!RegisterClassEx(&wcex))return 1;

	hWnd=CreateWindow(
		szWindowClass,
		szTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,CW_USEDEFAULT,
		CW_USEDEFAULT,CW_USEDEFAULT,
		NULL,NULL,
		hInstance,
		NULL
		);

	if(!hWnd)return 1;

	if(!Initialize(hWnd))return 1;

	while(GetMessage(&msg,NULL,0,0)){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}
