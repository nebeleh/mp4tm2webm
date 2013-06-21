// 2013-06 - Saman
// reading a video tree structure, appending black frames to mp4 files
// and converting them to webm, and creating video tree structure for new files
// requires ffmpeg and vpxenc, accessible from binary folder
// $ ./converter sourcevideos_path destvideos_path targetBitsPerPixel

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <regex.h>
#include <string>
#include <cmath>

using namespace std;

char buf[2000];
char destTop[100];
int width, height, fps, targetBitsPerPixel;

inline long lround(double x) {
  return (x > 0.0) ? (long)floor(x + 0.5) : (long)ceil(x - 0.5);
}

bool file_exists(const char *path)
{
  struct stat s;
  return (0 == stat(path, &s));
}

bool findReg(const char *s, const char *p, int &ansb, int &anse)
{
  regex_t re;
  regmatch_t rm;
  int status;
  regcomp(&re, p, REG_EXTENDED); 
  status = regexec(&re, s, 1, &rm, 0);
  regfree(&re);
  if (status)
  {
    cout << "r.json doesn't have required information." << endl;
    return 0;
  }
  ansb = rm.rm_so;
  anse = rm.rm_eo;
  return 1;
}

// loading a directory, convertin mp4s and copying everything to new directory
int loadData(const char *sourcePath, const char *destPath)
{
	struct dirent *entry;
	DIR *dp;

	dp = opendir(sourcePath);
	if (dp == NULL)
	{
		perror("opendir");
		return -1;
	}

	string f,t,fullPath;
	struct stat st;
	while((entry = readdir(dp)))
	{
		f = (string)entry->d_name;
    if (f == "." || f == "..") continue;
		fullPath = (string)sourcePath+'/'+f;
		stat(fullPath.c_str(), &st);
		
		// if this is a directory or a symlink, go inside it and make a similar structure in destination
		if((S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode)))
		{
      mkdir(((string)destPath+'/'+f).c_str(),0777);
			loadData(fullPath.c_str(),((string)destPath+'/'+f).c_str());
		}
    // if this is an mp4 file, append black video, convert it to webm and copy it
		else if(S_ISREG(st.st_mode) && f.length() > 4 && f.substr(f.length()-3)=="mp4")
		{
			// adding black frames
      cout << fullPath << ": adding black frames, " << flush;
      sprintf(buf,"./ffmpeg -y -i %s/black.mp4 -c copy -bsf:v h264_mp4toannexb -f mpegts %s/temp2 > /dev/null 2>&1 & ./ffmpeg -y -i %s -c copy -bsf:v h264_mp4toannexb -f mpegts %s/temp1 > /dev/null 2>&1 & ./ffmpeg -y -f mpegts -i \"concat:%s/temp1|%s/temp2\" -c copy %s/%s > /dev/null 2>&1",destTop,destTop,fullPath.c_str(),destTop,destTop,destTop,destPath,f.c_str());
      system(buf);
      
      // converting to webm
      cout << "encoding to yuv, " << flush;
      sprintf(buf,"./ffmpeg -i %s/%s -pix_fmt yuv420p %s/%s.yuv > /dev/null 2>&1", destPath, f.c_str(), destPath, f.c_str());
      
      system(buf);
      
      cout << "decoding to webm, pass 1, " << flush;
      sprintf(buf,"./vpxenc %s/%s.yuv -o %s/%s.webm -w %d -h %d --fps=%ld/1 -p 2 --codec=vp8 --fpf=%s/%s.fpf --cpu-used=0 --target-bitrate=%ld --auto-alt-ref=1 -v --min-q=4 --max-q=52 --drop-frame=0 --lag-in-frames=25 --kf-min-dist=0 --kf-max-dist=120 --static-thresh=0 --tune=psnr --pass=1 --minsection-pct=0 --maxsection-pct=800 -t 0 > /dev/null 2>&1", destPath, f.c_str(), destPath, f.substr(0,f.length()-4).c_str(), width, height, lround(fps), destPath, f.substr(0,f.length()-4).c_str(), lround(width * height * fps * targetBitsPerPixel / 8000.0));
  
      system(buf);
      
      cout << "pass 2, " << flush;
      sprintf(buf,"./vpxenc %s/%s.yuv -o %s/%s.webm -w %d -h %d --fps=%ld/1 -p 2 --codec=vp8 --fpf=%s/%s.fpf --cpu-used=0 --target-bitrate=%ld --auto-alt-ref=1 -v --min-q=4 --max-q=52 --drop-frame=0 --lag-in-frames=25 --kf-min-dist=0 --kf-max-dist=120 --static-thresh=0 --tune=psnr --pass=2 --minsection-pct=5 --maxsection-pct=1000 --bias-pct=50 -t 6 --end-usage=vbr --good --profile=0 --arnr-maxframes=7 --arnr-strength=5 --arnr-type=3 --psnr > /dev/null 2>&1", destPath, f.c_str(), destPath, f.substr(0,f.length()-4).c_str(), width, height, lround(fps), destPath, f.substr(0,f.length()-4).c_str(), lround(width * height * fps * targetBitsPerPixel / 8000.0));
      
      system(buf);
      
      // deleting mp4, fpf and yuv files
      cout << "deleting temp files, " << flush;
      sprintf(buf,"rm %s/%s.m* %s/%s.fpf -f", destPath, f.substr(0,f.length()-4).c_str(), destPath, f.substr(0,f.length()-4).c_str());
      
      system(buf);
      cout << "done!" << endl;
		}
    // otherwise copy it
    else
    {
      sprintf(buf,"cp %s %s",fullPath.c_str(),destPath);
      cout << fullPath << ": copying" << endl;
      system(buf);
    }
	}

	closedir(dp);
	return 0;
}

// do some preparation
int prepareData(const char *sourcePath, const char *destPath)
{
  // check whether ./ffmpeg & ./vpxenc exist
  if (!file_exists("ffmpeg") || !file_exists("vpxenc"))
  {
    cout << "Didn't find ffmpeg and/or vpxenc in local folder" << endl;
    return -1;
  }
  
  // check whether source directory exists
  if (!file_exists(sourcePath))
  {
    cout << "Source folder doesn't exists." << endl;
    return -1;
  }
  
  // check whether source directory has r.json
  sprintf(buf,"%s/r.json",sourcePath);
  if (!file_exists(buf))
  {
    cout << "r.json doesn't exist in source folder." << endl;
    return -1;
  }
  
  // read r.json and find width and height of video
  FILE *in = fopen(buf, "rb");
  if (in)
  {
    long len;
    char *contents;
    fseek(in,0,SEEK_END); //go to end
    len=ftell(in); //get position at end (length)
    fseek(in,0,SEEK_SET); //go to beg.
    contents=(char *)malloc(len); //malloc buffer
    fread(contents,len,1,in); //read into buffer
    fclose(in);
    
    int a,b;
    if (!findReg(contents,"video_height\\S\\S[0-9]+",a,b))
      return -1;
    memset(buf,0,sizeof(buf));
    strncpy(buf,contents+a+14,b-a-14);
    height = atoi(buf);
    
    if (!findReg(contents,"video_width\\S\\S[0-9]+",a,b))
      return -1;
    memset(buf,0,sizeof(buf));
    strncpy(buf,contents+a+13,b-a-13);
    width = atoi(buf);
    
    if (!findReg(contents,"fps\\S\\S\\S[0-9]+",a,b))
      return -1;
    memset(buf,0,sizeof(buf));
    strncpy(buf,contents+a+6,b-a-6);
    fps = atoi(buf);
  }
  else
  {
    cout << "Cannot open r.json" << endl;
    return -1;
  }
  
  // check for destination directory; it shouldn't exist
  if (file_exists(destPath))
  {
    cout << "Destination folder already exists. Please first remove it." << endl;
    return -1;
  }
  
  // create destination directory
  cout << "Creating destination folder ..." << endl;
  umask(~0777);
  if (mkdir(destPath, 0777))
  {
    perror("mkdir");
    return -1;
  }
  
  // create a 10 frame, 1 second black video in correct width and height
  sprintf(buf,"./ffmpeg -t 1 -s %dx%d -vcodec rawvideo -f rawvideo -pix_fmt rgb24 -r 10 -i /dev/zero -vcodec libx264 -preset slow -pix_fmt yuv420p %s/black.mp4 > /dev/null 2>&1",width,height,destPath);
  cout << "Creating black frames ..." << endl;
  system(buf);
  
  // create fifo files
  cout << "Creating temporary fifo files ..." << endl;
  sprintf(buf,"mkfifo %s/temp1 %s/temp2",destPath,destPath);
  system(buf);
  
  // recursively check for files, convert mp4s and copy them
  sprintf(destTop,"%s",destPath);
  int res = loadData(sourcePath, destPath);
  
  // delete fifo files and black video
  cout << "Deleting temporary files ..." << endl;
  sprintf(buf,"rm -f %s/temp1 %s/temp2 %s/black.mp4",destPath,destPath,destPath);
  system(buf);
  
  // finish
  return res;
}

int main(int argc, char **argv)
{
  // check whether there are right amount of parameters
  if (argc != 4)
  {
    cout << "incorrect parameters." << endl;
    cout << "usage: ./converter sourcevideos_path destvideos_path targetBitsPerPixel" << endl;
  }
  
  // do computations using source and destination folder
  targetBitsPerPixel = atoi(argv[3]);
  int res = prepareData(argv[1], argv[2]);
  
  if (!res)
    cout << "Converting finished successfully." << endl;
  
  return res;
}








