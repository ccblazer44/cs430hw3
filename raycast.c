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
    double *difColor;
    double *specColor;
    double *position;
    double *normal;
    double radius;
} Object;

Object objects[128]; //128 objects per assignment

//struct for holding lights
typedef struct {
    char *type;
    double *position;
    double *color;
    double radialA0;
    double radialA1;
    double radialA2;
    double theta;
    double angularA0;
    double *direction; 
} Light;

Light lights[128]; //array of lights

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
  int objectIndex = -1;
  int lightIndex = -1;
  int lightPosition = 0; //variable to keep track of light or object because they both have a "position" variable
  while (1) {
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
      } else if (strcmp(value, "sphere") == 0) {
        objectIndex++;
        objects[objectIndex].type = "sphere";
        lightPosition = 0;
      } else if (strcmp(value, "plane") == 0) {
        objectIndex++;
        objects[objectIndex].type = "plane";
        lightPosition = 0;
      } else if (strcmp(value, "light") == 0) {
        lightIndex++;
        lights[lightIndex].type = "light";
        lightPosition = 1;
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
            objects[objectIndex].radius = next_number(json);
          }
          else if (strcmp(key, "diffuse_color") == 0) {
            objects[objectIndex].difColor = next_vector(json);
          }
          else if (strcmp(key, "specular_color") == 0) {
            objects[objectIndex].specColor = next_vector(json);
          }
          else if (strcmp(key, "position") == 0) {
            if(lightPosition == 1){
              lights[lightIndex].position = next_vector(json);
            }
            else{
              objects[objectIndex].position = next_vector(json);
            }
          }
          else if (strcmp(key, "normal") == 0) {
            objects[objectIndex].normal = next_vector(json);
          }
          else if (strcmp(key, "color") == 0) {
            lights[lightIndex].color = next_vector(json);
          }
          else if (strcmp(key, "radial-a0") == 0) {
            lights[lightIndex].radialA0 = next_number(json);
          }
          else if (strcmp(key, "radial-a1") == 0) {
            lights[lightIndex].radialA1 = next_number(json);
          }
          else if (strcmp(key, "radial-a2") == 0) {
            lights[lightIndex].radialA2 = next_number(json);
          }
          else if (strcmp(key, "theta") == 0) {
            lights[lightIndex].theta = next_number(json);
          }
          else if (strcmp(key, "angular-a0") == 0) {
            lights[lightIndex].angularA0 = next_number(json);
          }
          else if (strcmp(key, "direction") == 0) {
            lights[lightIndex].direction = next_vector(json);
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


void tClosestApproachPlane(){

}

void tClosestApproachSphere(){

}

void vectorAdd(){

}

void vectorSub(){

}

void vectorMult(){

}

void vectorDiv(){

}

void vectorDot(){

}

void vector

void vectorUnit(){

}

void raycast() {

  int i;
  int j;
  int index = 0;
  int objectIndex;

  //loop through all pixels
  for(i = 0; i < Height; i++){
    for(j = 0; j < Width; j++){

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

      double min = 999999999999999999; //set min so that close objects display over further ones
      objectIndex = 0;
      double t;

      double poi[3];
      //loop through all objects
      while(objects[objectIndex].difColor != NULL){

        if(strcmp(objects[objectIndex].type, "sphere") == 0){

          //use unit vector to calculate collision
          t = (((x * objects[objectIndex].position[0]) + (y * objects[objectIndex].position[1]) + (z * objects[objectIndex].position[2]))/(pow(x, 2) + pow(y, 2) + pow(z, 2)));

          //find point on vector closest to center of sphere
          double tCloseX = x * t;
          double tCloseY = y * t;
          double tCloseZ = z * t;
          double d = sqrt(pow((tCloseX - objects[objectIndex].position[0]), 2) + pow((tCloseY - objects[objectIndex].position[1]), 2) + pow((tCloseZ - objects[objectIndex].position[2]), 2));

          //check if point is closer than radius (if there is an intersection)
          if(d <= objects[objectIndex].radius){

            //find distance from camera to actual intersection point and set it to t
            double a = sqrt(pow(objects[objectIndex].radius, 2) - pow(d, 2));
            t = t - a;

            //set new min so that close spheres display over further ones
            if (t > 0 && min >= t){
              min = t;
              viewPlane[index] = objects[objectIndex].difColor; //push color into viewPane
              poi[0] = t * x;
              poi[1] = t * y;
              poi[2] = t * z;
            }
          }
        }
        else if(strcmp(objects[objectIndex].type, "plane") == 0){

          //use unit vector to calculate collision
          t = -(objects[objectIndex].normal[0] * (0 - objects[objectIndex].position[0]) + objects[objectIndex].normal[1] * (0 - objects[objectIndex].position[1]) + objects[objectIndex].normal[2] * (0 - objects[objectIndex].position[2])) / (objects[objectIndex].normal[0] * x + objects[objectIndex].normal[1] * y + objects[objectIndex].normal[2] * z);

          if (t > 0 && min >= t) {
            viewPlane[index] = objects[objectIndex].difColor; //push color into viewPane
            min = t;
            poi[0] = t * x;
            poi[1] = t * y;
            poi[2] = t * z;
          }
        }
        objectIndex++;
      }
      int lightIndex = 0;


      //WORKING HEREvvvv


      //loop through lights
      while(lights[lightIndex].color != NULL){

        int lightVector[3];
        int lightUnitVector[3];

        lightVector[0] = poi[0] - lights[lightIndex].position[0];
        lightVector[1] = poi[1] - lights[lightIndex].position[1];
        lightVector[2] = poi[2] - lights[lightIndex].position[2];


        double lightT =  sqrt(pow(lightVector[0], 2) + pow(lightVector[1], 2) + pow(lightVector[2], 2));


        int objectIndex2 = 0;
        t = -1;
        //loop through objects to find shadows
        while(objects[objectIndex2].difColor != NULL){

          if(strcmp(objects[objectIndex2].type, "sphere") == 0){

            //use unit vector to calculate collision
            t = (((x * objects[objectIndex2].position[0]) + (y * objects[objectIndex2].position[1]) + (z * objects[objectIndex2].position[2]))/(pow(x, 2) + pow(y, 2) + pow(z, 2)));

            //find point on vector closest to center of sphere
            double tCloseX = x * t;
            double tCloseY = y * t;
            double tCloseZ = z * t;
            double d = sqrt(pow((tCloseX - objects[objectIndex2].position[0]), 2) + pow((tCloseY - objects[objectIndex2].position[1]), 2) + pow((tCloseZ - objects[objectIndex2].position[2]), 2));

            //check if point is closer than radius (if there is an intersection)
            if(d <= objects[objectIndex2].radius){

              //find distance from camera to actual intersection point and set it to t
              double a = sqrt(pow(objects[objectIndex2].radius, 2) - pow(d, 2));
              t = t - a;

              //set new min so that close spheres display over further ones
              if (t > 0 && min >= t){
                min = t;
                viewPlane[index] = objects[objectIndex2].difColor; //push color into viewPane
                poi[0] = t * x;
                poi[1] = t * y;
                poi[2] = t * z;
              }
            }
          }
          else if(strcmp(objects[objectIndex2].type, "plane") == 0){

            //use unit vector to calculate collision
            t = -(objects[objectIndex2].normal[0] * (0 - objects[objectIndex2].position[0]) + objects[objectIndex2].normal[1] * (0 - objects[objectIndex2].position[1]) + objects[objectIndex2].normal[2] * (0 - objects[objectIndex2].position[2])) / (objects[objectIndex2].normal[0] * x + objects[objectIndex2].normal[1] * y + objects[objectIndex2].normal[2] * z);

            if (t > 0 && min >= t) {
              viewPlane[index] = objects[objectIndex2].difColor; //push color into viewPane
              min = t;
              poi[0] = t * x;
              poi[1] = t * y;
              poi[2] = t * z;
            }
          }
          objectIndex2++;

        }
      }


      // WORKING HERE ^^^^^

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

void testPrint(){

  printf("testing\n");

  int index = 0;

  while(objects[index].type != NULL){

    if (objects[index].type != NULL) {
      printf("type: %s\n", objects[index].type);
    }

    if (objects[index].radius != 0) {
      printf("radius: %f\n", objects[index].radius);
    }

    if (objects[index].difColor != NULL) {
      printf("difColor: %f, %f, %f\n", objects[index].difColor[0], objects[index].difColor[1], objects[index].difColor[2]);
    }

    if (objects[index].specColor != NULL) {
      printf("specColor: %f, %f, %f\n", objects[index].specColor[0], objects[index].specColor[1], objects[index].specColor[2]);
    }

    if (objects[index].position != NULL) {
      printf("postion: %f, %f, %f\n", objects[index].position[0], objects[index].position[1], objects[index].position[2]);
    }
    if (objects[index].normal != NULL) {
      printf("normal: %f, %f, %f\n", objects[index].normal[0], objects[index].normal[1], objects[index].normal[2]);
    }
    index++;
    printf("\n\n");
  }


  index = 0;

  while (lights[index].type != NULL) {

    if (lights[index].type != NULL) {
      printf("type: %s\n", lights[index].type);
    }
    if (lights[index].position != NULL) {
      printf("postion: %f, %f, %f\n", lights[index].position[0], lights[index].position[1], lights[index].position[2]);
    }
    if (lights[index].color != NULL) {
      printf("color: %f, %f, %f\n", lights[index].color[0], lights[index].color[1], lights[index].color[2]);
    }
    if (lights[index].direction != NULL) {
      printf("direction: %f, %f, %f\n", lights[index].direction[0], lights[index].direction[1], lights[index].direction[2]);
    }
    if (lights[index].radialA0 != 0) {
      printf("radialA0: %f\n", lights[index].radialA0);
    }
    if (lights[index].radialA1 != 0) {
      printf("radialA1: %f\n", lights[index].radialA1);
    }
    if (lights[index].radialA2 != 0) {
      printf("radialA2: %f\n", lights[index].radialA2);
    }
    if (lights[index].theta != 0) {
      printf("theta: %f\n", lights[index].theta);
    }
    if (lights[index].angularA0 != 0) {
      printf("angularA0: %f\n", lights[index].angularA0);
    }
    index++;
    printf("\n\n");
  }
  
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

    //testPrint();

    raycast();
    write_scene(argv[4], 3);
    return 0;
}