#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>


//#define DEBUG
#define AUTHOR "CBLAZER"
#define RGB_NUMBER 255

//struct for holding objects
typedef struct {
    char *type;
    double *color;
    double *position;
    double *normal;
    double radius;
} Object;

Object objects[128]; //128 objects per assignment

int line = 1;
int Width;
int Height;
int color;
double cameraWidth;
double cameraHeight;
double **viewPlane; //2d array to store pixel color data from raycast
double white[3] = {1,1,1};

//number maintenance
int next_c(FILE* json) {
  int c = fgetc(json);
#ifdef DEBUG
  printf("next_c: '%c'\n", c);
#endif
  if (c == '\n') {
    line += 1;
  }
  if (c == EOF) {
    fprintf(stderr, "Error: Unexpected end of file on line number %d.\n", line);
    exit(1);
  }
  return c;
}

//expect_c() checks that the next character is d.  If it is not it emits an error.
void expect_c(FILE* json, int d) {
  int c = next_c(json);
  if (c == d) return;
  fprintf(stderr, "Error: Expected '%c' on line %d.\n", d, line);
  exit(1);
}

//skip_ws() skips white space in the file.
void skip_ws(FILE* json) {
  int c = next_c(json);
  while (isspace(c)) {
    c = next_c(json);
  }
  ungetc(c, json);
}

//next_string() gets the next string from the file handle and emits an error if a string can not be obtained.
char* next_string(FILE* json) {
  char buffer[129];
  int c = next_c(json);
  if (c != '"') {
    fprintf(stderr, "Error: Expected string on line %d.\n", line);
    exit(1);
  }
  c = next_c(json);
  int i = 0;
  while (c != '"') {
    if (i >= 128) {
      fprintf(stderr, "Error: Strings longer than 128 characters in length are not supported.\n");
      exit(1);
    }
    if (c == '\\') {
      fprintf(stderr, "Error: Strings with escape codes are not supported.\n");
      exit(1);
    }
    if (c < 32 || c > 126) {
      fprintf(stderr, "Error: Strings may contain only ascii characters.\n");
      exit(1);
    }
    buffer[i] = c;
    i += 1;
    c = next_c(json);
  }
  buffer[i] = 0;
  return strdup(buffer);
}

double next_number(FILE* json) {
  double value;
  fscanf(json, "%lf", &value);
  return value;
}

double* next_vector(FILE* json) {
  double* v = malloc(3*sizeof(double));
  expect_c(json, '[');
  skip_ws(json);
  v[0] = next_number(json);
  skip_ws(json);
  expect_c(json, ',');
  skip_ws(json);
  v[1] = next_number(json);
  skip_ws(json);
  expect_c(json, ',');
  skip_ws(json);
  v[2] = next_number(json);
  skip_ws(json);
  expect_c(json, ']');
  return v;
}

void read_scene(char* filename) {
  int c;
  FILE* json = fopen(filename, "r");

  if (json == NULL) {
    fprintf(stderr, "Error: Could not open file \"%s\"\n", filename);
    exit(1);
  }

  skip_ws(json);

  //Find the beginning of the list
  expect_c(json, '[');

  skip_ws(json);

  //Find the objects
  int index = -1;
  while (1) {
    index++;
    c = fgetc(json);
    if (c == ']') {
      fprintf(stderr, "Error: This is the worst scene file EVER.\n");
      fclose(json);
      return;
    }
    if (c == '{') {
      skip_ws(json);

      //Parse the object
      char* key = next_string(json);
      if (strcmp(key, "type") != 0) {
        fprintf(stderr, "Error: Expected \"type\" key on line number %d.\n", line);
        exit(1);
      }

      skip_ws(json);

      expect_c(json, ':');

      skip_ws(json);

      char* value = next_string(json);

      if (strcmp(value, "camera") == 0) {
        //Do nothing, camera isn't an object in the scene.
        index--;
      } else if (strcmp(value, "sphere") == 0) {
        objects[index].type = "sphere";
      } else if (strcmp(value, "plane") == 0) {
        objects[index].type = "plane";
      } else {
        fprintf(stderr, "Error: Unknown type, \"%s\", on line number %d.\n", value, line);
        exit(1);
      }

      skip_ws(json);

      while (1) {

    c = next_c(json);
    if (c == '}') {
      //stop parsing this object
      break;
    } else if (c == ',') {
      //read another field
      skip_ws(json);
      char* key = next_string(json);
      skip_ws(json);
      expect_c(json, ':');
      skip_ws(json);
      if (strcmp(key, "width") == 0) {
        cameraWidth = next_number(json);
      }
      else if (strcmp(key, "height") == 0) {
        cameraHeight = next_number(json);
      }
      else if (strcmp(key, "radius") == 0) {
        objects[index].radius = next_number(json);
      }
      else if (strcmp(key, "color") == 0) {
        objects[index].color = next_vector(json);
      }
      else if (strcmp(key, "position") == 0) {
        objects[index].position = next_vector(json);
      }
      else if (strcmp(key, "normal") == 0) {
        objects[index].normal = next_vector(json);
      }
      else {
        fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",
          key, line);
      }
      skip_ws(json);
    } else {
      fprintf(stderr, "Error: Unexpected value on line %d\n", line);
      exit(1);
    }
      }
      skip_ws(json);
      c = next_c(json);
      if (c == ',') {

  skip_ws(json);
      } else if (c == ']') {
  fclose(json);
  return;
      } else {
        fprintf(stderr, "Error: Expecting ',' or ']' on line %d.\n", line);
        exit(1);
      }
    }
  }
}


void raycast() {

  int i;
  int j;
  int index = 0;
  int objectIndex;

  //loop through all pixels
  for(i = Height; i > 0; i--){
    for(j = Width; j > 0; j--){

      double x, y, z = 1; //z is always 1 because the view plane is 1 unit away from camera

      //find vector
      x = 0 - (cameraWidth/2) + ((cameraWidth/Width)*(j + 0.5));
      y = 0 - (cameraHeight/2) + ((cameraHeight/Height)*(i + 0.5));

      //calculate magnitude using "distance formula"
      double magnitude = sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));

      //replace vector with unit vector
      x = x/magnitude;
      y = y/magnitude;
      z = z/magnitude;
      double d;

      double min = 999999999999999999; //set min so that close objects display over further ones
      objectIndex = 0;
      double t;

      //loop through all objects
      while(objects[objectIndex].color != NULL){

        if(strcmp(objects[objectIndex].type, "sphere") == 0){

          double a, b, c;

          a = (pow(x, 2) + pow(y, 2) + pow(z, 2));

          b = 2 * (x * (0 - objects[objectIndex].position[0]) + y * (0 - objects[objectIndex].position[1]) + z * (0 - objects[objectIndex].position[2]));

          c = pow((0 - objects[objectIndex].position[0]), 2) + pow((0 - objects[objectIndex].position[1]), 2) + pow((0 - objects[objectIndex].position[2]), 2) - pow(objects[objectIndex].radius, 2);

          double t0, t1, t;

          t0 = (-b - pow((pow(b, 2) - 4*c*a), 0.5)) / 2*a;
          t1 = (-b + pow((pow(b, 2) - 4*c*a), 0.5)) / 2*a;

          if (t0 < 0){
            t = t1;
          }
          else{
            t = t0;
          }

          if (min >= t){
              min = t;
              viewPlane[index] = objects[objectIndex].color; //push color into viewPane
          }
 
        }
        else if(strcmp(objects[objectIndex].type, "plane") == 0){

          double a, b, c;

          a = objects[objectIndex].normal[0];
          b = objects[objectIndex].normal[1];
          c = objects[objectIndex].normal[2];

          double magnitude = sqrt(pow(a, 2) + pow(b, 2) + pow(c, 2));

         //normalize normal vector
          a = a/magnitude;
          b = b/magnitude;
          c = c/magnitude;

          d = -((a * objects[objectIndex].position[0]) + (b * objects[objectIndex].position[1]) + (c * objects[objectIndex].position[2]));

          t = -(d)/(x*a + y*b + z*c);
          if (min >= t) {
            min = t;
            viewPlane[index] = objects[objectIndex].color; //push color into viewPane
          }
        }
        objectIndex++;
      }
      if(min == 999999999999999999){
        viewPlane[index] = white;
      }
      index++;
    }
  }
}


void write_scene(char *filename, int format) {

  FILE *ppm = fopen(filename, "wb");
  if (!ppm) {
    fprintf(stderr, "Error opening ppm file\n");
    exit(1);
  }

  //header
  if (format == 6) {
    fprintf(ppm, "P6\n");
  }
  else if (format == 3) {
    fprintf(ppm, "P3\n");
  }

  fprintf(ppm, "# Created by %s\n", AUTHOR);
  fprintf(ppm, "%d %d\n", Width, Height);
  fprintf(ppm, "%d\n", RGB_NUMBER);


  //image data
  int index;
  if (format == 6) {
    for (index = 0; index < Width * Height; index++) {

      color = (int) (viewPlane[index][0] * 255); //cast color to int from the double stroed in viewPane
      fwrite(&color, 1, 1, ppm); //red
      color = (int) (viewPlane[index][1] * 255);
      fwrite(&color, 1, 1, ppm); //green
      color = (int) (viewPlane[index][2] * 255);
      fwrite(&color, 1, 1, ppm); //blue
    }
  }
  else if (format == 3) {
    for (index = 0; index < Width * Height; index++) {

      color = (int) (viewPlane[index][0] * 255);
      fprintf(ppm, "%d\n", color);
      color = (int) (viewPlane[index][1] * 255);
      fprintf(ppm, "%d\n", color);
      color = (int) (viewPlane[index][2] * 255);
      fprintf(ppm, "%d\n", color);
    }
  }

  fclose(ppm);
}


int main(int argc, char** argv) {

  if (argc != 5){
        printf("usage: raycast width height input.json output.ppm");
        return(1);
    }

    Width = atoi(argv[1]);
    Height = atoi(argv[2]);

    read_scene(argv[3]);

    viewPlane = (double **)malloc(Width * Height * 3 * sizeof(double));
    raycast();
    write_scene(argv[4], 6);
    return 0;
}
