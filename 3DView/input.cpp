#include<stdio.h>
#include<stdint.h>
#include<string>
#include<vector>
#include<sstream>
#include<iostream>
#include"Vector.h"
#include"random32.h"

struct Cluster{
	Vector r,v,z;
	double size,spin;
	int n;
	Cluster(const std::string &src);
	void create(float (*pos)[8]);
};

double mystrtolf(const std::string &str){
	double result=-1.0;
	std::string s(str);
	s.erase(s.find_last_not_of(" \n\r\t")+1);
	std::istringstream ss(s);
	ss>>result;
	if(!ss.eof())return -1.0;
	return result;
}
int mystrtoi(const std::string &str){
	int result=-1;
	std::string s(str);
	s.erase(s.find_last_not_of(" \n\r\t")+1);
	std::istringstream ss(s);
	ss>>result;
	if(!ss.eof())return -1;
	return result;
}
int mystrtoVector(const std::string &str,Vector &result){
	std::string s(str);
	s.erase(s.find_last_not_of(" \n\r\t")+1);
	std::istringstream ss(s);
	char ch=0;
	ss>>result.x;
	ss>>ch;
	if(ch!=',')return 1;
	ch=0;
	ss>>result.y;
	ss>>ch;
	if(ch!=',')return 1;
	ch=0;
	ss>>result.z;
	return !ss.eof();
}
int resolve(const std::string &src,std::vector<std::string> &res){
	std::string cmd;
	int space=0;
	for(size_t i=0;i<src.size();++i){
		char ch=src[i];
		if(ch>='A'&&ch<='Z')ch+=32;
		if(ch>='a'&&ch<='z'){
			if(cmd.size()&&space){
				printf(" syntax error:%s should followed by '='\n\n",cmd.c_str());
				return 1;
			}
			cmd+=ch;
			space=0;
		}
		else if(ch==' '||ch=='\t'||ch=='\r'||ch=='\n')space=1;
		else if(ch==','){
			if(cmd.size()){
				printf(" syntax error: ','\n\n");
				return 1;
			}
			space=1;
		}
		else if(ch=='='){
			res.resize(res.size()+1);
			res.back().swap(cmd);
			int lbr=0,rbr=0,have=0;
			size_t start=0;
			for(i=i+1;i<src.size();++i){
				ch=src[i];
				if(ch=='{'){
					++lbr;
				}
				else if(ch=='}'){
					--lbr;
					if(lbr==0)break;
					if(lbr<0){
						printf(" Error: brackets are not matched.\n\n");
						return 1;
					}
				}
				else if(ch==' '||ch=='\t'||ch=='\r'||ch=='\n'){
					if(!lbr&&start)break;
				}
				else if(ch==','){
					if(!start)start=i;
					if(!lbr)break;
				}
				else {
					if(!start)start=i;
				}
			}
			if(start)res.push_back(src.substr(start,i-start));
		}
		else{
			printf(" Error:unrecognized char:%c\n\n",ch);
			return 1;
		}
	}
	return 0;
}
Cluster::Cluster(const std::string &src):r(0,0,0),v(0,0,0),z(0,0,1),n(-1),size(-1.0),spin(-1.0){
	std::vector<std::string> cmdlist;
	const char *cmds2[]={"n","size","spin","position","velocity","axis"};
	if(resolve(src,cmdlist)==0){
		for(size_t i=0;i<cmdlist.size();i+=2){
			int idx=0;
			int invalid=0;
			do {
				if(!strcmp(cmds2[idx],cmdlist[i].c_str()))break;
			}while(++idx<6);
			if(idx==6)printf(" Warning: skipping unrecognized var %s\n\n",cmdlist[i].c_str());
			if(idx==0){
				int vali=mystrtoi(cmdlist[i+1]);
				if(vali<0)invalid=1;
				n=vali;
			}
			else if(idx<3){
				double vald=mystrtolf(cmdlist[i+1]);
				if(vald<0)invalid=1;
				if(idx==1)size=vald;
				else if(idx==2)spin=vald;
			}
			else{
				Vector valv;
				invalid=mystrtoVector(cmdlist[i+1],valv);
				if(idx==3)r=valv;
				else if(idx==4)v=valv;
				else if(idx==5)z=valv;
			}
			if(invalid){
				printf(" Error: invalid value of %s:{%s}\n",cmdlist[i].c_str(),cmdlist[i+1].c_str());
				n=-1;
				return;
			}
		}
	}
}
void Cluster::create(float (*pos)[8]){
	double hpi=pi*0.5;
	Coordinate coor(perpunit(z),0,z);
	for(size_t i=0;i<n;++i){
		double theta=dpi*random(),phi=asin(2*random()-1);
		double rd=sqrt(abs(randomN())),vd=erf(rd*rd*cos(phi)*cos(phi)/sqrt(2.0));
		double vtheta=pi*random();
		rd*=size;
		vd=sqrt((2-spin)*n*vd/rd);
		Vector rv(theta,phi),vv(theta+hpi,0.0);
		vv=sin(spin*hpi)*vv+sin(vtheta)*rv*cos(spin*hpi);
		rv.z*=1-spin;
		rv=coor.inver(rv*rd)+r;
		vv=coor.inver(vv*vd)+v;
		pos[i][0]=rv.x;
		pos[i][1]=rv.y;
		pos[i][2]=rv.z;
		pos[i][3]=1.0;
		pos[i][4]=vv.x;
		pos[i][5]=vv.y;
		pos[i][6]=vv.z;
		pos[i][7]=1.0;
	}
}

int initsystem(int workdim,float &deltaTime,float &softSq,float &damping,float (*&pos)[8]){
	FILE *initf=fopen("initial.txt","rb");
	if(!initf){
		printf(" Error: cannot open initial.txt\n\n");
		return 0;
	}

	_fseeki64(initf,0,SEEK_END);
	size_t ifsize=_ftelli64(initf);
	_fseeki64(initf,0,SEEK_SET);

	std::string fstr(ifsize,0);
	for(size_t i=0;i<ifsize;++i){
		int ch=fgetc(initf);
		if(ch==EOF){
			printf(" Error: cannot read initial.txt.\n\n");
			fclose(initf);
			return 0;
		}
		fstr[i]=ch;
	}
	fclose(initf);
	
	std::vector<std::string> cmdlist;
	std::vector<Cluster> clsts;
	if(resolve(fstr,cmdlist))return 0;

	const char *cmds1[]={"deltatime","softening","damping","cluster","comment"};
	float softening=-1;
	deltaTime=-1;
	damping=-1;
	int num=0;

	for(size_t i=0;i<cmdlist.size();i+=2){
		int idx=0;
		do {
			if(!strcmp(cmds1[idx],cmdlist[i].c_str()))break;
		}while(++idx<5);
		if(idx==5)printf(" Warning: skipping unrecognized var %s\n\n",cmdlist[i].c_str());
		else if(idx==3){
			clsts.push_back(cmdlist[i+1]);
			if(clsts.back().n<=0)return 0;
			num+=clsts.back().n;
		}
		else if(idx<3){
			double val=mystrtolf(cmdlist[i+1].c_str());
			if(val<0){
				printf(" Error: invalid value of %s:{%s}\n",cmdlist[i].c_str(),cmdlist[i+1].c_str());
				return 0;
			}
			if(idx==0)deltaTime=val;
			else if(idx==1)softening=val;
			else if(idx==2)damping=val;
		}
	}

	if(num==0){
		std::cout<<" No cluster read.\n\n";
		return 0;
	}
	int diffnum=((num-1)/workdim+1)*workdim-num;
	while(diffnum){
		int insertid=random32()%num;
		int idcl=0;
		while((insertid-=clsts[idcl].n)>=0)++idcl;
		++clsts[idcl].n;
		++num;
		--diffnum;
	}

	if(deltaTime<=0){
		std::cout<<" Invalid deltaTime="<<deltaTime<<"\n\n";
		return 0;
	}
	if(softening<0){
		std::cout<<" Invalid softening="<<softening<<"\n\n";
		return 0;
	}
	if(damping<0){
		std::cout<<" Invalid damping="<<damping<<"\n\n";
		return 0;
	}
	std::cout
		<<"deltaTime = "<<deltaTime<<"\n"
		<<"softening = "<<softening<<"\n"
		<<"  damping = "<<damping<<"\n\n";
	softSq=softening*softening;

	for(size_t i=0;i<clsts.size();++i){
		std::cout<<"cluster "<<i+1<<" =\n";
		if(clsts[i].n<=0){
			std::cout<<" Invalid n="<<clsts[i].n<<"\n\n";
			return 0;
		}
		if(clsts[i].size<=0){
			std::cout<<" Invalid size="<<clsts[i].size<<"\n\n";
			return 0;
		}
		if(clsts[i].spin<0||clsts[i].spin>1){
			std::cout<<" Invalid spin="<<clsts[i].spin<<"\n\n";
			return 0;
		}
		double nz=norm(clsts[i].z);
		if(nz==0){
			std::cout<<" Invalid axis={0,0,0}\n\n";
			return 0;
		}
		std::cout
			<<"          n = "<<clsts[i].n<<"\n"
			<<"       size = "<<clsts[i].size<<"\n"
			<<"       spin = "<<clsts[i].spin<<"\n"
			<<"   position = {"<<clsts[i].r.x<<","<<clsts[i].r.y<<","<<clsts[i].r.z<<"}\n"
			<<"   velocity = {"<<clsts[i].v.x<<","<<clsts[i].v.y<<","<<clsts[i].v.z<<"}\n"
			<<"       axis = {"<<clsts[i].z.x<<","<<clsts[i].z.y<<","<<clsts[i].z.z<<"}\n\n";
		clsts[i].z/=nz;
	}
	std::cout<<"total num = "<<num<<"\n";

	pos=new float[num][8];
	diffnum=0;
	for(size_t i=0;i<clsts.size();++i){
		clsts[i].create(pos+diffnum);
		diffnum+=clsts[i].n;
	}
	return num;
}
