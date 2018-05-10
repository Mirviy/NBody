#ifndef VECTOR_H
#define VECTOR_H
//namespace LosAyior{
	//数学准备
	class Vector;
	class Coordinate;
	const double pi = 3.1415926535897932;
	const double dpi = 6.2831853071795864;
	const double rad = 180/pi;
	inline double round(double x){
		double y=floor(x);
		if(x-y>=0.5)y+=1.;
		return y;
	}
	inline double sqr(double x){
		return x*x;
	}
	/*
		Vector:三维向量,*代表数乘或叉乘,%代表点乘.
		+,+=,-,-=,*,*=,/,/=,%.
		norm 取模, getlat 取纬度, getlon 取经度.
	*/
	class Vector{
	public:
		double x,y,z;
		Vector(){}
		inline Vector(double x,double y, double z):x(x),y(y),z(z){}
		inline Vector(double theta,double phi){
			double c=cos(phi);
			x=c*cos(theta);
			y=c*sin(theta);
			z=sin(phi);
		}
		inline Vector(const Coordinate &coor,double x0,double y0,double z0);
		inline Vector(const Coordinate &coor,double theta,double phi);
		inline void setzero(){x=0;y=0;z=0;}
		inline void rotx(double b){
			double s=sin(b),c=cos(b),ny=y*c-z*s;
			z=z*c+y*s;
			y=ny;
		}
		inline void roty(double b){
			double s=sin(b),c=cos(b),nz=z*c-x*s;
			x=x*c+z*s;
			z=nz;
		}
		inline void rotz(double b){
			double s=sin(b),c=cos(b),nx=x*c-y*s;
			y=y*c+x*s;
			x=nx;
		}
	};
	inline double norm(const Vector &a){
		return sqrt(a.x*a.x+a.y*a.y+a.z*a.z);
	}
	inline double getlon(const Vector &a){
		return (a.x==0&&a.y==0)?0:atan2(a.y,a.x);
	}
	inline double getlat(const Vector &a){
		return (a.x==0&&a.y==0&&a.z==0)?0:atan2(a.z,sqrt(a.x*a.x+a.y*a.y));
	}
	inline Vector operator -(const Vector &a){
		return Vector(-a.x,-a.y,-a.z);
	}
	inline Vector &operator +=(Vector &a,const Vector &b){
		a.x+=b.x;
		a.y+=b.y;
		a.z+=b.z;
		return a;
	}
	inline Vector &operator -=(Vector &a,const Vector &b){
		a.x-=b.x;
		a.y-=b.y;
		a.z-=b.z;
		return a;
	}
	inline Vector &operator *=(Vector &a,const Vector &b){
		double x=a.y*b.z-a.z*b.y,y=a.z*b.x-a.x*b.z;
		a.z=a.x*b.y-a.y*b.x;
		a.x=x;
		a.y=y;
		return a;
	}
	inline Vector &operator *=(Vector &b,double a){
		b.x*=a;
		b.y*=a;
		b.z*=a;
		return b;
	}
	inline Vector &operator /=(Vector &b,double a){
		double reca=1/a;
		b.x*=reca;
		b.y*=reca;
		b.z*=reca;
		return b;
	}
	inline Vector operator +(const Vector &a,const Vector &b){
		return Vector(a.x+b.x,a.y+b.y,a.z+b.z);
	}
	inline Vector operator -(const Vector &a,const Vector &b){
		return Vector(a.x-b.x,a.y-b.y,a.z-b.z);
	}
	inline Vector operator *(const Vector &a,const Vector &b){
		return Vector(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);
	}
	inline Vector operator *(double a,const Vector &b){
		return Vector(a*b.x,a*b.y,a*b.z);
	}
	inline Vector operator *(const Vector &b,double a){
		return Vector(a*b.x,a*b.y,a*b.z);
	}
	inline Vector operator /(const Vector &b,double a){
		double reca=1/a;
		return Vector(b.x*reca,b.y*reca,b.z*reca);
	}
	inline double operator %(const Vector &a,const Vector &b){
		return a.x*b.x+a.y*b.y+a.z*b.z;
	}
	inline Vector perpunit(const Vector &z){
		Vector x(-z.y,z.x,0);
		if(x.y==0)x.x=1;
		else{
			double r=1/sqrt(x.x*x.x+x.y*x.y);
			x.x*=r;
			x.y*=r;
		}
		return x;
	}
	/*
		Coordinate:坐标系.
		trans可将标准坐标系下的向量转化为当前坐标系表达.
		inver可将当前坐标系下的向量转化为标准坐标系表达.
	*/
	class Coordinate{
	public:
		Vector x,y,z;
		inline Coordinate():x(1,0,0),y(0,1,0),z(0,0,1){}
		inline Coordinate(const Vector &x0,const Vector &y0,const Vector &z0):x(x0),y(y0),z(z0){}
		inline Coordinate(const Vector &x0,const Vector &y0,int):x(x0),y(y0){z=x*y;}
		inline Coordinate(const Vector &x0,int,const Vector &z0):x(x0),z(z0){y=z*x;}
		inline Coordinate(int,const Vector &y0,const Vector &z0):y(y0),z(z0){x=y*z;}
		inline Vector trans(const Vector &a){return Vector(a%x,a%y,a%z);}
		inline Vector inver(const Vector &a){return a.x*x+a.y*y+a.z*z;}
		inline void getlonlat(const Vector &a,double &lon,double &lat){
			Vector na(trans(a));
			lon=::getlon(na);
			lat=::getlat(na);
		}
		inline void rotx(double b){
			double s=sin(b),c=cos(b);
			Vector ny(y*c+z*s);
			z=z*c-y*s;
			y=ny;
		}
		inline void roty(double b){
			double s=sin(b),c=cos(b);
			Vector nz(z*c+x*s);
			x=x*c-z*s;
			z=nz;
		}
		inline void rotz(double b){
			double s=sin(b),c=cos(b);
			Vector nx(x*c+y*s);
			y=y*c-x*s;
			x=nx;
		}
		inline double getlon(const Vector &a){
			double ax(a%x),ay(a%y);
			return (ax==0&&ay==0)?0:atan2(ay,ax);
		}
		inline double getlat(const Vector &a){return ::getlat(trans(a));}
	};
	inline Vector::Vector(const Coordinate &coor,double x0,double y0,double z0){
		x=coor.x.x*x0+coor.y.x*y0+coor.z.x*z0;
		y=coor.x.y*x0+coor.y.y*y0+coor.z.y*z0;
		z=coor.x.z*x0+coor.y.z*y0+coor.z.z*z0;
	}
	inline Vector::Vector(const Coordinate &coor,double theta,double phi){
		double c=cos(phi),x0=c*cos(theta),y0=c*sin(theta),z0=sin(phi);
		x=coor.x.x*x0+coor.y.x*y0+coor.z.x*z0;
		y=coor.x.y*x0+coor.y.y*y0+coor.z.y*z0;
		z=coor.x.z*x0+coor.y.z*y0+coor.z.z*z0;
	}
//}
#endif
