#include <iostream>         // cout, cerr
#include <cstdlib>          // EXIT_FAILURE
#include <GL/glew.h>        // GLEW library
#include <GLFW/glfw3.h>     // GLFW library
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>      // Image loading Utility functions

// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "./tutorial_05_04/ShapeCreator.h"
#include "./tutorial_05_04/Mesh.h"
#include <camera.h> // Camera class

using namespace std; // Standard namespace

/*Shader program Macro*/
#ifndef GLSL
#define GLSL(Version, Source) "#version " #Version " core \n" #Source
#endif

// Unnamed namespace
namespace
{
const char* const WINDOW_TITLE = "CS330 Project"; // Macro for window title

// Variables for window width and height
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;


//scene to hold each shape
vector<GLMesh> scene;

// Main GLFW window
GLFWwindow* gWindow = nullptr;
// Texture
GLuint gTextureId;
glm::vec2 gUVScale(5.0f, 5.0f);
GLint gTexWrapMode = GL_REPEAT;

// camera
Camera gCamera(glm::vec3(0.0f, 10.0f, 50.0f));
float gLastX = WINDOW_WIDTH / 2.0f;
float gLastY = WINDOW_HEIGHT / 2.0f;
bool gFirstMouse = true;
bool perspective = true;

// timing
float gDeltaTime = 0.0f; // time between current frame and last frame
float gLastFrame = 0.0f;

// Shader program
GLuint gKeyLightId;
GLuint gSpotLightId;

GLMesh gSpotLightMesh;

// Light color, position and scale
glm::vec3 gKeyLightColor(1.0f, 1.0f, 1.0f);
glm::vec3 gKeyLightPosition(0.0f, 250.0f, -250.0f);
glm::vec3 gKeyLightScale(10.0f);

// Light color, position and scale
glm::vec3 gSpotLightColor(1.0f, 1.0f, 1.0f);
glm::vec3 gSpotLightPosition(-10.0f, 10.0f, -10.0f);
glm::vec3 gSpotLightScale(0.1f);

bool gSpotLightOrbit = true;
}

/* User-defined Function prototypes to:
 * initialize the program, set the window size,
 * redraw graphics on the window when resized,
 * and render graphics on the screen
 */
bool UInitialize(int, char*[], GLFWwindow** window);
void UResizeWindow(GLFWwindow* window, int width, int height);
void UProcessInput(GLFWwindow* window);
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos);
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void UDestroyMesh(GLMesh &mesh);
void UCreateScene(vector<GLMesh>& scene);
bool UCreateTexture(const char* filename, GLuint &textureId);
void UDestroyTexture(GLuint textureId);
void URender(vector<GLMesh>& scene);
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint &programId);
void UDestroyShaderProgram(GLuint programId);


/* Cube Vertex Shader Source Code*/
const GLchar * keyVertexShaderSource = GLSL(440,

    layout (location = 0) in vec3 position; // VAP position 0 for vertex position data
    layout (location = 1) in vec3 normal; // VAP position 1 for normals
    layout (location = 2) in vec2 textureCoordinate;

    out vec3 vertexNormal; // For outgoing normals to fragment shader
    out vec3 vertexFragmentPos; // For outgoing color / pixels to fragment shader
    out vec2 vertexTextureCoordinate;

    //Uniform / Global variables for the  transform matrices
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main()
    {
        gl_Position = projection * view * model * vec4(position, 1.0f); // Transforms vertices into clip coordinates

        vertexFragmentPos = vec3(model * vec4(position, 1.0f)); // Gets fragment / pixel position in world space only (exclude view and projection)

        vertexNormal = mat3(transpose(inverse(model))) * normal; // get normal vectors in world space only and exclude normal translation properties
        vertexTextureCoordinate = textureCoordinate;
    }
);


/* Cube Fragment Shader Source Code*/
const GLchar * keyFragmentShaderSource = GLSL(440,

    in vec3 vertexNormal; // For incoming normals
    in vec3 vertexFragmentPos; // For incoming fragment position
    in vec2 vertexTextureCoordinate;

    out vec4 fragmentColor; // For outgoing cube color to the GPU

    // Uniform / Global variables for object color, light color, light position, and camera/view position
    uniform vec3 objectColor;
    uniform vec3 lightColor;
    uniform vec3 lightPos;
    uniform vec3 viewPosition;
    uniform sampler2D uTexture; // Useful when working with multiple textures
    uniform vec2 uvScale;

    void main()
    {
        /*Phong lighting model calculations to generate ambient, diffuse, and specular components*/

        //Calculate Ambient lighting*/
        float ambientStrength = 0.4f; // Set ambient or global lighting strength
        vec3 ambient = ambientStrength * lightColor; // Generate ambient light color

        //Calculate Diffuse lighting*/
        vec3 norm = normalize(vertexNormal); // Normalize vectors to 1 unit
        vec3 lightDirection = normalize(lightPos - vertexFragmentPos); // Calculate distance (light direction) between light source and fragments/pixels on cube
        float impact = max(dot(norm, lightDirection), 0.0);// Calculate diffuse impact by generating dot product of normal and light
        vec3 diffuse = impact * lightColor; // Generate diffuse light color

        //Calculate Specular lighting*/
        float specularIntensity = 0.8f; // Set specular light strength
        float highlightSize = 16.0f; // Set specular highlight size
        vec3 viewDir = normalize(viewPosition - vertexFragmentPos); // Calculate view direction
        vec3 reflectDir = reflect(-lightDirection, norm);// Calculate reflection vector
        //Calculate specular component
        float specularComponent = pow(max(dot(viewDir, reflectDir), 0.0), highlightSize);
        vec3 specular = specularIntensity * specularComponent * lightColor;

        // Texture holds the color to be used for all three components
        vec4 textureColor = texture(uTexture, vertexTextureCoordinate * uvScale);

        // Calculate phong result
        vec3 phong = (ambient + diffuse + specular) * textureColor.xyz;

        fragmentColor = vec4(phong, 1.0); // Send lighting results to GPU
    }
);


/* Lamp Shader Source Code*/
const GLchar * spotVertexShaderSource = GLSL(440,

    layout (location = 0) in vec3 position; // VAP position 0 for vertex position data

        //Uniform / Global variables for the  transform matrices
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main()
    {
        gl_Position = projection * view * model * vec4(position, 1.0f); // Transforms vertices into clip coordinates
    }
);


/* Fragment Shader Source Code*/
const GLchar * spotFragmentShaderSource = GLSL(440,

    out vec4 fragmentColor; // For outgoing lamp color (smaller cube) to the GPU

    void main()
    {
        fragmentColor = vec4(1.0f); // Set color to white (1.0f,1.0f,1.0f) with alpha 1.0
    }
);


// Images are loaded with Y axis going down, but OpenGL's Y axis goes up, so let's flip it
void flipImageVertically(unsigned char *image, int width, int height, int channels)
{
    for (int j = 0; j < height / 2; ++j)
    {
        int index1 = j * width * channels;
        int index2 = (height - 1 - j) * width * channels;

        for (int i = width * channels; i > 0; --i)
        {
            unsigned char tmp = image[index1];
            image[index1] = image[index2];
            image[index2] = tmp;
            ++index1;
            ++index2;
        }
    }
}


int main(int argc, char* argv[])
{
    if (!UInitialize(argc, argv, &gWindow))
        return EXIT_FAILURE;

    // Create the scene
    UCreateScene(scene);

    // Create the shader programs
    if (!UCreateShaderProgram(keyVertexShaderSource, keyFragmentShaderSource, gKeyLightId))
        return EXIT_FAILURE;

    if (!UCreateShaderProgram(spotVertexShaderSource, spotFragmentShaderSource, gSpotLightId))
        return EXIT_FAILURE;

    for (auto& m : scene)
    {
        if (!UCreateTexture(m.texFilename, m.textureId))
        {
            cout << "Failed to load texture " << m.texFilename << endl;
            //cin.get();
            return EXIT_FAILURE;

        }
    }

    // tell opengl for each sampler to which texture unit it belongs to (only has to be done once)
    glUseProgram(gKeyLightId);
    // We set the texture as texture unit 0
    glUniform1i(glGetUniformLocation(gKeyLightId, "uTexture"), 0);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(gWindow))
    {
        // per-frame timing
        // --------------------
        float currentFrame = glfwGetTime();
        gDeltaTime = currentFrame - gLastFrame;
        gLastFrame = currentFrame;

        // input
        // -----
        UProcessInput(gWindow);

        // Render this frame
        URender(scene);

        glfwPollEvents();
    }

    //clean up
    for (auto& m : scene) {
        UDestroyMesh(m);
    }

    scene.clear();

    // Release texture
    UDestroyTexture(gTextureId);

    // Release shader program
    UDestroyShaderProgram(gKeyLightId);
    UDestroyShaderProgram(gSpotLightId);

    exit(EXIT_SUCCESS); // Terminates the program successfully
}


// Initialize GLFW, GLEW, and create a window
bool UInitialize(int argc, char* argv[], GLFWwindow** window)
{
    // GLFW: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // GLFW: window creation
    // ---------------------
    *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (*window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(*window);
    glfwSetFramebufferSizeCallback(*window, UResizeWindow);
    glfwSetCursorPosCallback(*window, UMousePositionCallback);
    glfwSetScrollCallback(*window, UMouseScrollCallback);
    glfwSetMouseButtonCallback(*window, UMouseButtonCallback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // GLEW: initialize
    // ----------------
    // Note: if using GLEW version 1.13 or earlier
    glewExperimental = GL_TRUE;
    GLenum GlewInitResult = glewInit();

    if (GLEW_OK != GlewInitResult)
    {
        std::cerr << glewGetErrorString(GlewInitResult) << std::endl;
        return false;
    }

    // Displays GPU OpenGL version
    cout << "INFO: OpenGL Version: " << glGetString(GL_VERSION) << endl;

    return true;
}


// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void UProcessInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    //Moves the camera according to what keys are pressed
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        gCamera.ProcessKeyboard(FORWARD, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        gCamera.ProcessKeyboard(BACKWARD, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        gCamera.ProcessKeyboard(LEFT, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        gCamera.ProcessKeyboard(RIGHT, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        gCamera.ProcessKeyboard(DOWN, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        gCamera.ProcessKeyboard(UP, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        perspective = (perspective ? false : true); // changes view type
    }

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && gTexWrapMode != GL_REPEAT)
    {
        glBindTexture(GL_TEXTURE_2D, gTextureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);

        gTexWrapMode = GL_REPEAT;

        cout << "Current Texture Wrapping Mode: REPEAT" << endl;
    }
    else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && gTexWrapMode != GL_MIRRORED_REPEAT)
    {
        glBindTexture(GL_TEXTURE_2D, gTextureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);

        gTexWrapMode = GL_MIRRORED_REPEAT;

        cout << "Current Texture Wrapping Mode: MIRRORED REPEAT" << endl;
    }
    else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && gTexWrapMode != GL_CLAMP_TO_EDGE)
    {
        glBindTexture(GL_TEXTURE_2D, gTextureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        gTexWrapMode = GL_CLAMP_TO_EDGE;

        cout << "Current Texture Wrapping Mode: CLAMP TO EDGE" << endl;
    }
    else if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS && gTexWrapMode != GL_CLAMP_TO_BORDER)
    {
        float color[] = {1.0f, 0.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);

        glBindTexture(GL_TEXTURE_2D, gTextureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glBindTexture(GL_TEXTURE_2D, 0);

        gTexWrapMode = GL_CLAMP_TO_BORDER;

        cout << "Current Texture Wrapping Mode: CLAMP TO BORDER" << endl;
    }

    if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
    {
        gUVScale += 0.1f;
        cout << "Current scale (" << gUVScale[0] << ", " << gUVScale[1] << ")" << endl;
    }
    else if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)
    {
        gUVScale -= 0.1f;
        cout << "Current scale (" << gUVScale[0] << ", " << gUVScale[1] << ")" << endl;
    }

    // Pause and resume lamp orbiting
    static bool isLKeyDown = false;
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS && !gSpotLightOrbit)
        gSpotLightOrbit = true;
    else if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS && gSpotLightOrbit)
        gSpotLightOrbit = false;
}


// glfw: whenever the window size changed (by OS or user resize) this callback function executes
void UResizeWindow(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}


// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (gFirstMouse)
    {
        gLastX = xpos;
        gLastY = ypos;
        gFirstMouse = false;
    }

    float xoffset = xpos - gLastX;
    float yoffset = gLastY - ypos; // reversed since y-coordinates go from bottom to top

    gLastX = xpos;
    gLastY = ypos;

    gCamera.ProcessMouseMovement(xoffset, yoffset);
}


// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    gCamera.ProcessMouseScroll(yoffset);
}

// glfw: handle mouse button events
// --------------------------------
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    switch (button)
    {
        case GLFW_MOUSE_BUTTON_LEFT:
        {
            if (action == GLFW_PRESS)
                cout << "Left mouse button pressed" << endl;
            else
                cout << "Left mouse button released" << endl;
        }
        break;

        case GLFW_MOUSE_BUTTON_MIDDLE:
        {
            if (action == GLFW_PRESS)
                cout << "Middle mouse button pressed" << endl;
            else
                cout << "Middle mouse button released" << endl;
        }
        break;

        case GLFW_MOUSE_BUTTON_RIGHT:
        {
            if (action == GLFW_PRESS)
                cout << "Right mouse button pressed" << endl;
            else
                cout << "Right mouse button released" << endl;
        }
        break;

        default:
            cout << "Unhandled mouse button event" << endl;
            break;
    }
}

// creates each mesh using ShapeCreator class
void UCreateScene(vector<GLMesh>& scene) {
    // Folder plane
    GLMesh gPlane;
    gPlane.p = {
        1.0f, 1.0f, 1.0f, 1.0f,				// color r, g, b a
        28.0f, 1.0f, 20.0f,					// scale x, y, z
        0.0f, 1.0f, 0.0f, 0.0f,				// x amount of rotation, rotate x, y, z
        0.0f, 0.0f, 1.0f, 0.0f,			    // y amount of rotation, rotate x, y, z
        0.0f, 0.0f, 0.0f, 1.0f,				// z amount of rotation, rotate x, y, z
        0.0f, 0.0f, 0.0f,					// translate x, y, z
        1.0f, 1.0f                          // texture scale
    };
    gPlane.texFilename = "textures\\FolderTexture.png";
    ShapeCreator::UBuildPlane(gPlane);
    scene.push_back(gPlane);
  
    //SSD body
    GLMesh gSSDBody;
    gSSDBody.p = {
        1.0f, 1.0f, 1.0f, 1.0f,				// color r, g, b a
        4.5f, 1.0f, 7.5f,					// scale x, y, z
        0.0f, 1.0f, 0.0f, 0.0f,				// x amount of rotation, rotate x, y, z
        90.0f, 0.0f, 1.0f, 0.0f,			// y amount of rotation, rotate x, y, z
        0.0f, 0.0f, 0.0f, 1.0f,				// z amount of rotation, rotate x, y, z
        -10.0f, 0.0f, -10.0f,				// translate x, y, z
        1.0f, 1.0f                          // texture scale
    };
    gSSDBody.texFilename = "textures\\SSDtexture.jpg";
    ShapeCreator::UBuildCube(gSSDBody);
    scene.push_back(gSSDBody);

    //SSD edge 1
    GLMesh gSSDEdge1;
    gSSDEdge1.p = {
        1.0f, 1.0f, 1.0f, 1.0f,				// color r, g, b a
        1.0f, 1.0f, 1.0f,					// scale x, y, z
        0.0f, 1.0f, 0.0f, 0.0f,				// x amount of rotation, rotate x, y, z
        90.0f, 0.0f, 1.0f, 0.0f,			// y amount of rotation, rotate x, y, z
        0.0f, 0.0f, 0.0f, 1.0f,				// z amount of rotation, rotate x, y, z
        -13.75f, 0.0f, -11.75f,				// translate x, y, z
        1.0f, 1.0f
    };

    gSSDEdge1.length = 7.5f;	gSSDEdge1.radius = 0.5f;	gSSDEdge1.number_of_sides = 30.0f;
    gSSDEdge1.texFilename = "textures\\SSDtexture.jpg";
    ShapeCreator::UBuildCylinder(gSSDEdge1);
    scene.push_back(gSSDEdge1);

    //SSD edge 2
    GLMesh gSSDEdge2;
    gSSDEdge2.p = {
        1.0f, 1.0f, 1.0f, 1.0f,				// color r, g, b a
        1.0f, 1.0f, 1.0f,					// scale x, y, z
        0.0f, 1.0f, 0.0f, 0.0f,				// x amount of rotation, rotate x, y, z
        90.0f, 0.0f, 1.0f, 0.0f,			// y amount of rotation, rotate x, y, z
        0.0f, 0.0f, 0.0f, 1.0f,				// z amount of rotation, rotate x, y, z
        -13.75f, 0.0f, -7.25f,				// translate x, y, z
        1.0f, 1.0f
    };

    gSSDEdge2.length = 7.5f;	gSSDEdge2.radius = 0.5f;	gSSDEdge2.number_of_sides = 30.0f;
    gSSDEdge2.texFilename = "textures\\SSDtexture.jpg";
    ShapeCreator::UBuildCylinder(gSSDEdge2);
    scene.push_back(gSSDEdge2);

    //Tape measure body
    GLMesh gTapeMeasureBody;
    gTapeMeasureBody.p = {
        1.0f, 1.0f, 1.0f, 1.0f,				// color r, g, b a
        4.25f, 1.3f, 4.25f,					// scale x, y, z
        0.0f, 1.0f, 0.0f, 0.0f,				// x amount of rotation, rotate x, y, z
        45.0f, 0.0f, 1.0f, 0.0f,			// y amount of rotation, rotate x, y, z
        0.0f, 0.0f, 0.0f, 1.0f,				// z amount of rotation, rotate x, y, z
        10.0f, 0.0f, -10.0f,			    // translate x, y, z
        1.0f, 1.0f                          // texture scale
    };
    gTapeMeasureBody.texFilename = "textures\\blue.jpg";
    ShapeCreator::UBuildCube(gTapeMeasureBody);
    scene.push_back(gTapeMeasureBody);
    
    //Tape measure button
    GLMesh gTapeMeasureButton;
    gTapeMeasureButton.p = {
        1.0f, 1.0f, 1.0f, 1.0f,				// color r, g, b a
        1.0f, 1.0f, 1.0f,					// scale x, y, z
        -90.0f, 1.0f, 0.0f, 0.0f,			// x amount of rotation, rotate x, y, z
        0.0f, 0.0f, 1.0f, 0.0f,			    // y amount of rotation, rotate x, y, z
        0.0f, 0.0f, 0.0f, 1.0f,				// z amount of rotation, rotate x, y, z
        9.5f, 0.0f, -9.5f,			        // translate x, y, z
        1.0f, 1.0f
    };
    gTapeMeasureButton.length = 1.4f;	gTapeMeasureButton.radius = 0.5f;	gTapeMeasureButton.number_of_sides = 30.0f;
    gTapeMeasureButton.texFilename = "textures\\white.png";
    ShapeCreator::UBuildCylinder(gTapeMeasureButton);
    scene.push_back(gTapeMeasureButton);

    //Tape roll
    GLMesh gTapeRoll;
    gTapeRoll.p = {
        1.0f, 1.0f, 1.0f, 1.0f,				// color r, g, b a
        1.0f, 1.0f, 1.0f,					// scale x, y, z
        -90.0f, 1.0f, 0.0f, 0.0f,			// x amount of rotation, rotate x, y, z
        0.0f, 0.0f, 1.0f, 0.0f,			    // y amount of rotation, rotate x, y, z
        0.0f, 0.0f, 0.0f, 1.0f,				// z amount of rotation, rotate x, y, z
        10.0f, 0.0f, 10.0f,			        // translate x, y, z
        1.0f, 1.0f
    };
    gTapeRoll.length = 2.0f;	gTapeRoll.radius = 2.75f;	gTapeRoll.number_of_sides = 30.0f;
    gTapeRoll.texFilename = "textures\\white.png";
    ShapeCreator::UBuildCylinder(gTapeRoll);
    scene.push_back(gTapeRoll);

    //Tape holder
    GLMesh gTapeHolder;
    gTapeHolder.p = {
        1.0f, 1.0f, 1.0f, 1.0f,				// color r, g, b a
        1.0f, 1.0f, 1.0f,					// scale x, y, z
        -90.0f, 1.0f, 0.0f, 0.0f,			// x amount of rotation, rotate x, y, z
        0.0f, 0.0f, 1.0f, 0.0f,			    // y amount of rotation, rotate x, y, z
        0.0f, 0.0f, 0.0f, 1.0f,				// z amount of rotation, rotate x, y, z
        10.0f, 0.0f, 10.0f,			        // translate x, y, z
        1.0f, 1.0f
    };
    gTapeHolder.length = 2.5f;	gTapeHolder.radius = 1.25f;	gTapeHolder.number_of_sides = 30.0f;
    gTapeHolder.texFilename = "textures\\blackTex.jpg";
    ShapeCreator::UBuildCylinder(gTapeHolder);
    scene.push_back(gTapeHolder);

    //Tape holder tab
    GLMesh gTapeHolderTab;
    gTapeHolderTab.p = {
        1.0f, 1.0f, 1.0f, 1.0f,				// color r, g, b a
        1.0f, 1.0f, 1.0f,					// scale x, y, z
        -90.0f, 1.0f, 0.0f, 0.0f,			// x amount of rotation, rotate x, y, z
        0.0f, 0.0f, 1.0f, 0.0f,			    // y amount of rotation, rotate x, y, z
        0.0f, 0.0f, 0.0f, 1.0f,				// z amount of rotation, rotate x, y, z
        10.0f, 0.0f, 10.0f,			        // translate x, y, z
        1.0f, 1.0f
    };
    gTapeHolderTab.length = 3.0f;	gTapeHolderTab.radius = 0.25f;	gTapeHolderTab.number_of_sides = 30.0f;
    gTapeHolderTab.texFilename = "textures\\blackTex.jpg";
    ShapeCreator::UBuildCylinder(gTapeHolderTab);
    scene.push_back(gTapeHolderTab);

    //Pen body
    GLMesh gPenBody;
    gPenBody.p = {
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        -75.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, 0.01f, 1.0f,
        1.0f, 1.0f
    };
    gPenBody.length = 13.0f;	gPenBody.radius = 0.4f;	gPenBody.number_of_sides = 30.0f;
    gPenBody.texFilename = "textures\\blackTex.jpg";
    ShapeCreator::UBuildCylinder(gPenBody);
    scene.push_back(gPenBody);

    //Pen cap
    GLMesh gPenCap;
    gPenCap.p = {
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        -75.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, 0.0f, 1.0f,
        1.0f, 1.0f
    };
    gPenCap.length = 5.5f;	gPenCap.radius = 0.5f;	gPenCap.number_of_sides = 30.0f;
    gPenCap.texFilename = "textures\\blackTex.jpg";
    ShapeCreator::UBuildCylinder(gPenCap);
    scene.push_back(gPenCap);
}


// Functioned called to render a frame
void URender(vector<GLMesh>& scene)
{
    // Lamp orbits around the origin
    const float angularVelocity = glm::radians(45.0f);
    if (gSpotLightOrbit)
    {
        glm::vec4 newPosition = glm::rotate(angularVelocity * gDeltaTime, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::vec4(gSpotLightPosition, 1.0f);
        gSpotLightPosition.x = newPosition.x;
        gSpotLightPosition.y = newPosition.y;
        gSpotLightPosition.z = newPosition.z;
    }

    gSpotLightMesh.p = {
    0.0f, 1.0f, 0.0f, 1.0f,				// color r, g, b a
    5.0f, 1.0f, 5.0f,					// scale x, y, z
    0.0f, 1.0f, 0.0f, 0.0f,				// x amount of rotation, rotate x, y, z
    0.0f, 0.0f, 1.0f, 0.0f,			    // y amount of rotation, rotate x, y, z
    0.0f, 0.0f, 0.0f, 1.0f,				// z amount of rotation, rotate x, y, z
    0.0f, 0.0f, 0.0f,					// translate x, y, z
    1.0f, 1.0f                          // texture scale
    };
    ShapeCreator::UBuildPlane(gSpotLightMesh);

    // Enable z-depth
    glEnable(GL_DEPTH_TEST);
    
    // Clear the frame and z buffers
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    //Draw scene

    // 1. Scales the object by 2
    glm::mat4 scale = glm::scale(glm::vec3(1.0f, 1.0f, 1.0f));
    // 2. Rotates shape by 15 degrees in the x axis
    glm::mat4 rotation = glm::rotate(45.0f, glm::vec3(1.0, 1.0f, 1.0f));
    // 3. Place object at the origin
    glm::mat4 translation = glm::translate(glm::vec3(0.0f, 0.0f, 0.0f));
    // Model matrix: transformations are applied right-to-left order
    glm::mat4 model = translation * rotation * scale;

    // camera/view transformation
    glm::mat4 view = gCamera.GetViewMatrix();

    // Creates a perspective projection
    glm::mat4 projection;
    if (perspective) {
        projection = glm::perspective(glm::radians(gCamera.Zoom), (GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT, 0.1f, 100.0f);
    }
    else {
        projection = glm::ortho(-14.0f, 14.0f, -10.0f, 10.0f, 0.1f, 100.0f);
    }
    // Set the shader to be used
    glUseProgram(gKeyLightId);

    // Retrieves and passes transform matrices to the Shader program
    GLint modelLoc = glGetUniformLocation(gKeyLightId, "model");
    GLint viewLoc = glGetUniformLocation(gKeyLightId, "view");
    GLint projLoc = glGetUniformLocation(gKeyLightId, "projection");

    // loop to draw each shape individually
    for (auto i = 0; i < scene.size(); ++i)
    {
        auto mesh = scene[i];

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mesh.model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        GLint objectColorLoc = glGetUniformLocation(gKeyLightId, "objectColor");
        GLint lightColorLoc = glGetUniformLocation(gKeyLightId, "lightColor");
        GLint lightPositionLoc = glGetUniformLocation(gKeyLightId, "lightPos");
        GLint viewPositionLoc = glGetUniformLocation(gKeyLightId, "viewPosition");

        glUniform3f(objectColorLoc, gKeyLightColor.r, gKeyLightColor.g, gKeyLightColor.b);
        glUniform3f(lightColorLoc, gSpotLightColor.r, gSpotLightColor.g, gSpotLightColor.b);
        glUniform3f(lightPositionLoc, gSpotLightPosition.x, gSpotLightPosition.y, gSpotLightPosition.z);
        const glm::vec3 cameraPosition = gCamera.Position;
        glUniform3f(viewPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);

        GLint UVScaleLoc = glGetUniformLocation(gKeyLightId, "uvScale");
        glUniform2fv(UVScaleLoc, 1, glm::value_ptr(mesh.gUVScale));

        // activate vbo's within mesh's vao
        glBindVertexArray(mesh.vao);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mesh.textureId);

        // Draws the triangles
        glDrawArrays(GL_TRIANGLES, 0, mesh.nIndices);
    }

    //Draw spotlight

    glUseProgram(gSpotLightId);

    //transform lamp
    model = glm::translate(gSpotLightPosition) * glm::scale(gSpotLightScale);

    // Reference matrix uniforms from the Lamp Shader program
    modelLoc = glGetUniformLocation(gSpotLightId, "model");
    viewLoc = glGetUniformLocation(gSpotLightId, "view");
    projLoc = glGetUniformLocation(gSpotLightId, "projection");

    // Pass matrix data to the Lamp Shader program's matrix uniforms
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    glDrawArrays(GL_TRIANGLES, 0, gSpotLightMesh.nIndices);


    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
    glfwSwapBuffers(gWindow);    // Flips the the back buffer with the front buffer every frame.
}


void UDestroyMesh(GLMesh &mesh)
{
    glDeleteVertexArrays(1, &mesh.vao);
    glDeleteBuffers(1, &mesh.vbo);
}


/*Generate and load the texture*/
bool UCreateTexture(const char* filename, GLuint &textureId)
{
    int width, height, channels;
    unsigned char *image = stbi_load(filename, &width, &height, &channels, 0);
    if (image)
    {
        flipImageVertically(image, width, height, channels);

        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);

        // set the texture wrapping parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        // set texture filtering parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (channels == 3)
        	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        else if (channels == 4)
        	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
        else
        {
        	cout << "Not implemented to handle image with " << channels << " channels" << endl;
        	return false;
        }

		glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(image);
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

        return true;
    }

    // Error loading the image
    return false;
}


void UDestroyTexture(GLuint textureId)
{
    glGenTextures(1, &textureId);
}


// Implements the UCreateShaders function
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint &programId)
{
    // Compilation and linkage error reporting
    int success = 0;
    char infoLog[512];

    // Create a Shader program object.
    programId = glCreateProgram();

    // Create the vertex and fragment shader objects
    GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

    // Retrive the shader source
    glShaderSource(vertexShaderId, 1, &vtxShaderSource, NULL);
    glShaderSource(fragmentShaderId, 1, &fragShaderSource, NULL);

    // Compile the vertex shader, and print compilation errors (if any)
    glCompileShader(vertexShaderId); // compile the vertex shader
    // check for shader compile errors
    glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShaderId, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;

        return false;
    }

    glCompileShader(fragmentShaderId); // compile the fragment shader
    // check for shader compile errors
    glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShaderId, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;

        return false;
    }

    // Attached compiled shaders to the shader program
    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);

    glLinkProgram(programId);   // links the shader program
    // check for linking errors
    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(programId, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;

        return false;
    }

    glUseProgram(programId);    // Uses the shader program

    return true;
}


void UDestroyShaderProgram(GLuint programId)
{
    glDeleteProgram(programId);
}
