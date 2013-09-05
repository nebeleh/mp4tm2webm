// 2013-06 - Saman

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include "picojson.h"
#include <string>
#include <cmath>
#include <ctime>

using namespace std;

char buf[2000];
char destTop[2000];
int width, height;
double targetBitsPerPixel, fps;
time_t instanceNumber;

inline long lround(double x) {
  return (x > 0.0) ? (long)floor(x + 0.5) : (long)ceil(x - 0.5);
}

bool file_exists(const char *path)
{
  struct stat s;
  return (0 == stat(path, &s));
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
    string tempDestPath = (string)destPath+'/'+f;

    // if this is a directory or a symlink, go inside it and make a similar structure in destination
    if((S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode)))
    {
      if(!file_exists(tempDestPath.c_str()))
      {
        umask(~0777);
        mkdir(tempDestPath.c_str(),0777);
      }
      loadData(fullPath.c_str(), tempDestPath.c_str());
    }
    // if this is an mp4 file, append black video, convert it to webm and copy it
    else if(S_ISREG(st.st_mode) && f.length() > 4 && f.substr(f.length()-4)==".mp4")
    {
      // do not convert video files with wrong name format or the ones already being converted by another process
      bool correctFormat = true;
      for (int i=0; i<f.length()-4; i++)
        if(f[i]-'0'<0 || f[i]-'0'>9)
        {
          correctFormat = false;
          break;
        }
      if (!correctFormat || file_exists(tempDestPath.c_str())) continue;

      // adding black frames
      cout << fullPath << ": adding black frames, " << flush;
      sprintf(buf,"./ffmpeg -y -i %s/black-%d.mp4 -c copy -bsf:v h264_mp4toannexb -f mpegts %s/temp2-%d &> /dev/null & ./ffmpeg -y -i %s -c copy -bsf:v h264_mp4toannexb -f mpegts %s/temp1-%d &> /dev/null & ./ffmpeg -y -f mpegts -i \"concat:%s/temp1-%d|%s/temp2-%d\" -c copy %s/%s &> /dev/null", destTop, instanceNumber, destTop, instanceNumber, fullPath.c_str(), destTop, instanceNumber, destTop, instanceNumber, destTop, instanceNumber, destPath,f.c_str());
      system(buf);

      // converting to webm
      cout << "encoding to yuv, " << flush;
      sprintf(buf,"./ffmpeg -i %s -pix_fmt yuv420p %s/%s-%d.yuv &> /dev/null", fullPath.c_str(), destPath, f.c_str(), instanceNumber);
      system(buf);

      cout << "decoding to webm, pass 1, " << flush;
      sprintf(buf,"./vpxenc %s/%s-%d.yuv -o %s/%s.webm -w %d -h %d --fps=%ld/1 -p 2 --codec=vp8 --fpf=%s/%s-%d.fpf --cpu-used=0 --target-bitrate=%ld --auto-alt-ref=1 -v --min-q=4 --max-q=52 --drop-frame=0 --lag-in-frames=25 --kf-min-dist=0 --kf-max-dist=120 --static-thresh=0 --tune=psnr --pass=1 --minsection-pct=0 --maxsection-pct=800 -t 0 &> /dev/null", destPath, f.c_str(), instanceNumber, destPath, f.substr(0,f.length()-4).c_str(), width, height, lround(fps), destPath, f.substr(0,f.length()-4).c_str(), instanceNumber, lround(width * height * fps * targetBitsPerPixel / 8000.0));
      system(buf);

      cout << "pass 2, " << flush;
      sprintf(buf,"./vpxenc %s/%s-%d.yuv -o %s/%s.webm -w %d -h %d --fps=%ld/1 -p 2 --codec=vp8 --fpf=%s/%s-%d.fpf --cpu-used=0 --target-bitrate=%ld --auto-alt-ref=1 -v --min-q=4 --max-q=52 --drop-frame=0 --lag-in-frames=25 --kf-min-dist=0 --kf-max-dist=120 --static-thresh=0 --tune=psnr --pass=2 --minsection-pct=5 --maxsection-pct=1000 --bias-pct=50 -t 6 --end-usage=vbr --good --profile=0 --arnr-maxframes=7 --arnr-strength=5 --arnr-type=3 --psnr &> /dev/null", destPath, f.c_str(), instanceNumber, destPath, f.substr(0,f.length()-4).c_str(), width, height, lround(fps), destPath, f.substr(0,f.length()-4).c_str(), instanceNumber, lround(width * height * fps * targetBitsPerPixel / 8000.0));
      system(buf);

      // deleting mp4, fpf and yuv files
      cout << "deleting temp files, " << flush;
      sprintf(buf,"rm %s/%s-%d.yuv %s/%s-%d.fpf -f", destPath, f.c_str(), instanceNumber, destPath, f.substr(0,f.length()-4).c_str(), instanceNumber);
      system(buf);
      cout << "done!" << endl;
    }
    // otherwise copy it
    else
    {
      cout << fullPath << ": copying" << endl;
      sprintf(buf,"cp %s %s",fullPath.c_str(),destPath);
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

    picojson::value v;
    string err;
    picojson::parse(v, contents, contents + strlen(contents), &err);
    if (! err.empty()) {
      cout << err << endl;
    }

    // check if the type of the value is "object"
    if (! v.is<picojson::object>()) {
      cout << "JSON is not an object" << endl;
      exit(2);
    }

    // obtain a const reference to the map, and print the contents
    height = -1;
    width = -1;
    fps = -1;
    const picojson::value::object& obj = v.get<picojson::object>();
    for (picojson::value::object::const_iterator i = obj.begin(); i != obj.end(); ++i) {
      if (i->first == "video_height") { height = atoi(i->second.to_str().c_str()); }
      if (i->first == "video_width") { width = atoi(i->second.to_str().c_str()); }
      if (i->first == "fps") { fps = atof(i->second.to_str().c_str()); }
    }

    if (height == -1 || width == -1 || fps == -1)
    {
      cout << "not enough inputs in JSON." << endl;
      return -1;
    }

    cout << "width: " << width << ", height: " << height << ", fps: " << fps << endl;
  }
  else
  {
    cout << "Cannot open r.json" << endl;
    return -1;
  }

  // create destination folders if they don't exist
  umask(~0777);
  if (!file_exists(destPath))
  {
    cout << "Creating destination folder ..." << endl;
    if (mkdir(destPath, 0777))
    {
      perror("mkdir");
      return -1;
    }
  }

  // create a 10 frame, 1 second black video in correct width and height
  sprintf(buf,"./ffmpeg -t 1 -s %dx%d -vcodec rawvideo -f rawvideo -pix_fmt rgb24 -r 10 -i /dev/zero -vcodec libx264 -preset slow -pix_fmt yuv420p %s/black-%d.mp4 > /dev/null 2>&1",width,height,destPath,instanceNumber);
  cout << "Creating black frames ..." << endl;
  system(buf);

  // create fifo files
  sprintf(destTop,"%s",destPath);
  cout << "Creating temporary fifo files ..." << endl;
  sprintf(buf,"mkfifo %s/temp1-%d %s/temp2-%d", destTop, instanceNumber, destTop, instanceNumber);
  system(buf);

  // recursively check for files, add black frames, convert mp4s
  int res = loadData(sourcePath, destPath);

  // delete fifo files and black video
  cout << "Deleting temporary files ..." << endl;
  sprintf(buf,"rm -f %s/temp1-%d %s/temp2-%d %s/black-%d.mp4", destTop, instanceNumber, destTop, instanceNumber, destTop, instanceNumber);
  system(buf);

  // finish
  return res;
}

int main(int argc, char **argv)
{
  // check whether there are right amount of parameters
  if (argc != 4)
  {
    cout << "incorrect parameters, usage: " << endl;
    cout << "./converter sourcevideos_path destvideos_path targetBitsPerPixel" << endl;
    return -1;
  }

  // do computations using source and destination folder
  instanceNumber = time(0);
  targetBitsPerPixel = atof(argv[3]);
  int res = prepareData(argv[1], argv[2]);

  if (!res)
    cout << "Converting finished successfully." << endl;
  else
    cout << "There was a problem during conversion." << endl;

  return res;
}

