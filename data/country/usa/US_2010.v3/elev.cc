#include <cstring>
#include <sstream>
#include <stdio.h>
#include <string>
#include <string.h>
#include <vector>
#include <math.h>

using namespace std;

typedef std::vector<int> int_vector_t;

int_vector_t grid[100][100];
std::vector<double> lat;
std::vector<double> lon;
std::vector<double> elev;
double dx;
double dy;
double minx;
double miny;
double maxx;
double maxy;

double get_elev(int i, int j, double lati, double loni) {
  int level = 0;
  int_vector_t cand;
  cand.clear();
  while (cand.size()==0 && level <= 100) {
    level++;
    int mini = (i < level) ? 0 : (i-level);
    int minj = (j < level) ? 0 : (j-level);
    int maxi = (100 <= i+level) ? 99 : (i+level);
    int maxj = (100 <= j+level) ? 99 : (j+level);
    for (int i = mini; i <= maxi; i++) {
      for (int j = minj; j <= maxj; j++) {
	cand.insert(cand.end(), grid[i][j].begin(), grid[i][j].end());
      }
    }
    // printf("level %d cand size %d\n", level, (int)cand.size());
  }
  double el = 0.0;
  double mindist = 99999999999999.0;
  int mink = -1;
  for (int i = 0; i < cand.size(); i++) {
    int k = cand[i];
    double dx = (loni - lon[k])/87.832;
    double dy = (lati - lat[k])/110.996;
    double dist = sqrt(dx*dx + dy*dy);
    if (dist < mindist) {
      mindist = dist;
      mink = k;
    }
  }
  if (mink > -1) {
    // printf("MINDIST %f %f || %f %f || %f  => %f\n", lati, loni, lat[mink], lon[mink], mindist, elev[mink]);
    el = elev[mink];
  }
  return el;
}

void get_households(char *fips) {
  double lati, loni, el;
  char line[1024];
  char file[1024];

  lat.clear();
  lon.clear();
  elev.clear();
  sprintf(file, "%s/households.txt", fips);
  FILE *fp = fopen(file, "r");
  fgets(line, 1024, fp);
  strcpy(line, "");
  fgets(line, 1024, fp);
  int items = sscanf(line, "%*s %*s %*s %*s %lf %lf %lf", &lati, &loni, &el);
  while (items == 3) {
    lat.push_back(lati);
    lon.push_back(loni);
    elev.push_back(el);
    strcpy(line, "");
    fgets(line, 1024, fp);
    items = sscanf(line, "%*s %*s %*s %*s %lf %lf %lf", &lati, &loni, &el);
  }
  fclose(fp);

  for (int i = 0; i < lon.size(); i++) {
    // printf("lat %f lon %f elev %f\n", lat[i],lon[i],elev[i]);
  }

}

void get_workplaces(char *fips) {
  double lati, loni, el;
  char line[1024];
  char file[1024];
  long long int spid;
  
  lat.clear();
  lon.clear();
  elev.clear();
  sprintf(file, "%s/workplaces.txt", fips);
  FILE *fp = fopen(file, "r");
  fgets(line, 1024, fp);
  strcpy(line, "");
  fgets(line, 1024, fp);
  int items = sscanf(line, "%lld %lf %lf %lf", &spid, &lati, &loni, &el);
  while (items == 4) {
    if (el != 0.0) {
      lat.push_back(lati);
      lon.push_back(loni);
      elev.push_back(el);
    }
    strcpy(line, "");
    fgets(line, 1024, fp);
    items = sscanf(line, "%lld %lf %lf %lf", &spid, &lati, &loni, &el);
  }
  fclose(fp);
}

void fix_workplaces(char *fips) {
  double lati, loni, el;
  char line[1024];
  char file[1024];
  long long int spid;
  
  sprintf(file, "%s/workplaces.txt", fips);
  FILE *fp = fopen(file, "r");
  fgets(line, 1024, fp);
  strcpy(line, "");
  fgets(line, 1024, fp);
  int items = sscanf(line, "%lld %lf %lf %lf", &spid, &lati, &loni, &el);
  while (items == 4) {
    if (el != 0.0) {
      // printf("OK: %lld %f %f %f\n", spid, lati, loni, el);
    }
    else {
      int i = (loni - minx) / dx;
      int j = (lati - miny) / dy;
      if (i < -20 || i > 120 || j < -20 || j > 120) {
	// printf("FAR: %lld %f %f %f\n", spid, lati, loni, el);
      }
      else {
	if (i < 0) i = 0;
	if (j < 0) j = 0;
	if (i > 99) i = 99;
	if (j > 99) j = 99;
	printf("FIX: i %d j %d %lld %f %f %f\n", i, j, spid, lati, loni, el);
	double new_elev = get_elev(i,j,lati,loni);
      }
    }
    strcpy(line, "");
    fgets(line, 1024, fp);
    items = sscanf(line, "%lld %lf %lf %lf", &spid, &lati, &loni, &el);
  }
  fclose(fp);
}

void get_elevation_of_hospitals(char *fips) {
  double lati, loni, el;
  char line[1024];
  char file[1024];
  char file2[1024];
  long long int spid;
  int workers = 0;
  int physicians = 0;
  int beds = 0;
  
  sprintf(file, "%s/hospitals.txt", fips);
  FILE *fp = fopen(file, "r");
  sprintf(file2, "%s/new-hospitals.txt", fips);
  FILE *OUT = fopen(file2, "w");
  strcpy(line, "");
  fgets(line, 1024, fp);
  line[strlen(line)-1] = '\0';
  fprintf(OUT, "%s\televation\n", line);
  strcpy(line, "");
  fgets(line, 1024, fp);
  int items = sscanf(line, "%lld %d %d %d %lf %lf", &spid, &workers, &physicians, &beds, &lati, &loni);
  while (items == 6) {
    // printf("PROCESSING %lld %d %d %d %f %f\n", spid, workers, physicians, beds, lati, loni);
    int i = (loni - minx) / dx;
    int j = (lati - miny) / dy;
    if (i < -100 || i > 200 || j < -100 || j > 200) {
      printf("FAR: %lld %f %f %f\n", spid, lati, loni, el);
      fprintf(OUT, "%lld\t%d\t%d\t%d\t%f\t%f\t%f\n", spid, workers, physicians, beds, lati, loni, 0.0);
    }
    else {
      if (i < 0) i = 0;
      if (j < 0) j = 0;
      if (i > 99) i = 99;
      if (j > 99) j = 99;
      printf("FIX: i %d j %d %lld %f %f\n", i, j, spid, lati, loni);
      double elev = get_elev(i,j,lati,loni);
      fprintf(OUT, "%lld\t%d\t%d\t%d\t%f\t%f\t%f\n", spid, workers, physicians, beds, lati, loni, elev);
    }
    strcpy(line, "");
    fgets(line, 1024, fp);
    items = sscanf(line, "%lld %d %d %d %lf %lf", &spid, &workers, &physicians, &beds, &lati, &loni);
  }
  fclose(fp);
  fclose(OUT);
  char cmd[1024];
  sprintf(cmd, "mv %s %s-old; mv %s %s", file, file, file2, file);
  system(cmd);
}

void get_range() {
  minx = -1;
  miny = -1;
  maxx = -1;
  maxy = -1;
  for (int i = 0; i < lat.size(); i++) {
    if (minx == -1 || lon[i] < minx) minx = lon[i];
    if (miny == -1 || lat[i] < miny) miny = lat[i];
    if (maxx == -1 || lon[i] > maxx) maxx = lon[i];
    if (maxy == -1 || lat[i] > maxy) maxy = lat[i];
  }
  dx = (maxx - minx) / 100.0;
  dy = (maxy - miny) / 100.0;
  printf("minx %f maxx %f miny %f maxy %f dx %f dy %f\n", minx,maxx,miny,maxy,dx,dy);
}

void get_grid() {
  int maxcount = 0;
  for (int k = 0; k < lat.size(); k++) {
    // printf("lat %f lon %f elev %f\n", lat[k],lon[k],elev[k]); fflush(stdout);
    int i = (lon[k] - minx) / dx;
    int j = (lat[k] - miny) / dy;
    if (i < -20 || i > 120 || j < -20 || j > 120) continue;
    if (i < 0) i = 0;
    if (j < 0) j = 0;
    if (i > 99) i = 99;
    if (j > 99) j = 99;
    // printf("lon %f lat %f i %d j %d\n", lon[k],lat[k],i,j); fflush(stdout);
    grid[i][j].push_back(k);
    // printf("lon %f lat %f i %d j %d count %d\n", lon[k],lat[k],i,j,(int)grid[i][j].size()); fflush(stdout);
    if (grid[i][j].size() > maxcount) maxcount = grid[i][j].size();
  }
  printf("maxcount = %d\n", maxcount);
}

#include <dirent.h>


int main() {

  DIR *d;
  struct dirent *dir;
  d = opendir(".");
  if (d) {
    while ((dir = readdir(d)) != NULL) {
      char line[1024];
      if (strlen(dir->d_name)!=5) continue;
      char fips[10];
      sprintf(fips, "%s", dir->d_name);
      char filename[100];
      sprintf(filename, "%s/hospitals.txt", fips);
      FILE *fp = fopen(filename, "r");
      if (fp==NULL) {
	printf("FIPS = %s has no hospital file\n", fips);
	continue;
      }
      int n = 0;
      while (fgets(line, 1024, fp)) { n++;}
      fclose(fp);
      /*
      if (n==1) {
	printf("%s has no hospitals\n", filename);
	continue;
      }
      */
      printf("FIPS = %s has %d hospitals\n", fips, n-1);
      for (int i = 0; i < 100; i++) {
	for (int j = 0; j < 100; j++) {
	  grid[i][j].clear();
	}
      }
      get_households(fips);
      get_range();
      get_grid();
      get_elevation_of_hospitals(fips);
    }
    closedir(d);
  }
  return(0);
}

