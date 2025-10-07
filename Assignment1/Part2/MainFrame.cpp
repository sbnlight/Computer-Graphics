#include "MainFrame.h"
#include <iostream>
#include <cmath>

namespace {

    float scale = 1.f;
    float aspect = 1.f;

#ifdef __APPLE__
    unsigned int SCR_WIDTH = 600;
    unsigned int SCR_HEIGHT = 600;
#else
    unsigned int SCR_WIDTH = 1000;
    unsigned int SCR_HEIGHT = 1000;
#endif

    void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
        scale *= std::pow(1.1f, (float) yoffset);
    }

    void FrameBufferSizeCallback(GLFWwindow *window, int width, int height) {
        SCR_WIDTH = width;
        SCR_HEIGHT = height;
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);        // Set the viewport to cover the new window
        aspect = (float) SCR_WIDTH /
                 (float) SCR_HEIGHT;  // Set the aspect ratio of the clipping area to match the viewport
    }
}

void MainFrame::LeftMouseMove(float start_x, float start_y, float curr_x, float curr_y) {
    if (modeling_state_ == OBJ_ROTATION) {
        // ---------------------------------- Object Rotation ---------------------------------------
        glm::mat4x4 transform_mat(1.f);

         // 1. find V on the screen
        glm::vec2 V = glm::vec2(curr_x - start_x, curr_y - start_y);
        
        // 2. rotate V to A by 90 degrees
        glm::vec2 A = glm::vec2(-V.y, V.x);
        
        // 3. find the rotation axis in the *world space*
        glm::vec3 start_world = Screen2World(start_x, start_y);
        glm::vec3 A_world = Screen2World(start_x + A.x, start_y + A.y);
        glm::vec3 rot_axis = glm::normalize(A_world - start_world);
        
        // 4. find the rotation angle k * ||A||, assign a proper value to k
        float k = 0.01f; // sensitivity factor
        float rot_angle = k * glm::length(A);

         // 5. find the rotation matrix
        glm::mat4x4 rot_mat = glm::rotate(glm::mat4x4(1.f), rot_angle, rot_axis);
        
        // 6. find the translation matrix
        glm::vec3 center = ActiveMeshCenter();
        glm::mat4x4 trans_to_origin = glm::translate(glm::mat4x4(1.f), -center);
        glm::mat4x4 trans_back = glm::translate(glm::mat4x4(1.f), center);
        
        // 7. find the final transformation matrix
        transform_mat = trans_back * rot_mat * trans_to_origin;

        mesh_.ApplyTransform(transform_mat);
    } else if (modeling_state_ == OBJ_TRANSLATION) {
        // ---------------------------------- Object Translation ------------------------------------
        glm::mat4x4 transform_mat(1.f);

        // 1. find the ray from the screen to the world space
        glm::vec3 o, v;
        std::tie(o, v) = Screen2WorldRay(start_x, start_y);
        // 2. find the intersection point with the object
        int face_id;
        glm::vec3 intersected_point;
        std::tie(face_id, intersected_point) = mesh_.FaceIntersection(o, v);
        if(face_id == -1) return;

        // 3. find the z value of the intersection point in the camera space
        float z_vals = World2Camera(intersected_point).z;

        // 4. find the translation vector
        glm::vec3 a = Screen2World(start_x, start_y, z_vals);
        glm::vec3 b = Screen2World(curr_x, curr_y, z_vals);

        // Remove camera-forward component to keep motion in view plane
        glm::vec3 camera_forward = CameraForward();
        glm::vec3 translation = b - a;
        translation = translation - glm::dot(translation, camera_forward) * camera_forward;

        // 5. find the translation matrix
        transform_mat = glm::translate(glm::mat4x4(1.f), translation);

        mesh_.ApplyTransform(transform_mat);
    } else if (modeling_state_ == OBJ_EXTRUDE) {
        // ---------------------------------- Face Extrusion ------------------------------------
        glm::mat4x4 transform_mat(1.f);
        int face_index = -1;

        // 1. find the intersection point with the face
        glm::vec3 o, v, v_cur;
        std::tie(o, v) = Screen2WorldRay(start_x, start_y);
        std::tie(o, v_cur) = Screen2WorldRay(curr_x, curr_y);
        glm::vec3 intersected_point;
        std::tie(face_index, intersected_point) = mesh_.FaceIntersection(o, v);
        if(face_index == -1) return;

        // 2. calculate the face normal vector in the world space using "mesh_.faces_" and "mesh_.vertices_"
        const auto& face = mesh_.faces_[face_index];
        glm::vec3 v0 = mesh_.vertices_[face[0]];
        glm::vec3 v1 = mesh_.vertices_[face[1]]; 
        glm::vec3 v2 = mesh_.vertices_[face[2]];
        glm::vec3 face_normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

        // 3. find the point on ray (intersected_point, face_normal) which is the closest to ray (o, v_cur)
        // Get the intersection between the extrusion ray and the mouse ray
        glm::vec3 extrusion_dir = face_normal;
        glm::vec3 p0 = intersected_point;
        glm::vec3 p1 = o;
        glm::vec3 u = extrusion_dir;
        glm::vec3 v_dir = glm::normalize(v_cur);
        
        // Find the closest points between two rays
        float a = glm::dot(u, u);
        float b = glm::dot(u, v_dir);
        float c = glm::dot(v_dir, v_dir);
        float d = glm::dot(u, p0 - p1);
        float e = glm::dot(v_dir, p0 - p1);
        float denom = a * c - b * b;
        
        if (std::abs(denom) < 1e-8f) return; // where rays are parallel
        
        float s = (b * e - c * d) / denom;
        glm::vec3 closest_point = p0 + s * u;

        // 4. find the translation matrix to move the face vertices along the face normal direction to the new point
        glm::vec3 translation = closest_point - intersected_point;
        transform_mat = glm::translate(glm::mat4x4(1.f), translation);

        mesh_.ApplyFaceTransform(face_index, transform_mat);
    } else if (modeling_state_ == OBJ_SCALE) {
        // ---------------------------------- Object Local Scaling ------------------------------------
        glm::mat4x4 transform_mat(1.f);

        float drag_distance = curr_y - start_y;
        float scale_factor = std::exp(drag_distance * 0.01f); // exponential scaling for smoothness
        
        // Get mesh center for scaling around center
        glm::vec3 center = ActiveMeshCenter();
        
        // Create scale transformation around center
        transform_mat = ComposeAroundPivot(center, glm::scale(glm::mat4x4(1.f), 
                                           glm::vec3(scale_factor, scale_factor, scale_factor)));

        mesh_.ApplyTransform(transform_mat);
    }
}

void MainFrame::VisualizeWorldSpace() {
    // ---------------------------------- World Space Visualization ------------------------------------
    glLineWidth(2.f);
    glBegin(GL_LINES);
    glColor3f(1.f, 0.f, 0.f);
    glVertex3f(0.f, 0.f, 0.f);
    glVertex3f(2.f, 0.f, 0.f);
    glColor3f(0.f, 1.f, 0.f);
    glVertex3f(0.f, 0.f, 0.f);
    glVertex3f(0.f, 2.f, 0.f);
    glColor3f(0.f, 0.f, 1.f);
    glVertex3f(0.f, 0.f, 0.f);
    glVertex3f(0.f, 0.f, 2.f);
    glEnd();
}

void MainFrame::MainLoop() {
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 4);

    // glfw window creation, set viewport with width=1000 and height=1000
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3DModeling", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, FrameBufferSizeCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    // glad: load all OpenGL function pointers
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    const float alpha = 0.3f;
    const float beta = 0.1f;

    const float r = 5.f;
    camera_.LookAt(r * glm::vec3(std::cos(alpha) * std::cos(beta), std::cos(alpha) * std::sin(beta), std::sin(alpha)),
                   glm::vec3(0.f, 0.f, 0.f),
                   glm::vec3(0.f, 0.f, 1.f));

    glEnable(GL_DEPTH_TEST);

    // render loop
    while (!glfwWindowShouldClose(window)) {
        ProcessInput(window);

        // Apply camera projection;
        camera_.Perspective(90.f, aspect, .5f, 10.f);
        camera_.UpdateScale(scale);
        scale = 1.f;
        camera_.ApplyProjection();

        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // Clear the display

        DrawScene();

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    // glfw: terminate, clearing addl previously allocated GLFW resources.
    glfwTerminate();
}

void MainFrame::ProcessInput(GLFWwindow *window) {
    // Key events
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
        modeling_state_ = OBJ_ROTATION;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        modeling_state_ = OBJ_TRANSLATION;
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
        modeling_state_ = OBJ_SUBDIVIDE;
    }
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) {
        modeling_state_ = OBJ_EXTRUDE;
    }
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
        modeling_state_ = OBJ_SCALE;
    }

    int current_l_mouse_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);

    // Handle left mouse
    if (current_l_mouse_state == GLFW_PRESS) {
        double xposd, yposd;
        float xpos, ypos;
        glfwGetCursorPos(window, &xposd, &yposd);
        xpos = float(xposd);
        ypos = float(SCR_HEIGHT - yposd);
        if (l_mouse_state_ == GLFW_RELEASE) {
            LeftMouseClick(xpos, ypos);
            l_click_cursor_x_ = xpos;
            l_click_cursor_y_ = ypos;
        }
        if (l_mouse_state_ == GLFW_PRESS &&
            (std::abs(xpos - last_cursor_x_) > 2.f || std::abs(ypos - last_cursor_y_) > 2.f)) {
            LeftMouseMove(l_click_cursor_x_, l_click_cursor_y_, xpos, ypos);
        }
        last_cursor_x_ = float(xpos);
        last_cursor_y_ = float(ypos);
    }
    if (current_l_mouse_state == GLFW_RELEASE) {
        if (l_mouse_state_ == GLFW_PRESS) {
            LeftMouseRelease();
        }
    }
    l_mouse_state_ = current_l_mouse_state;

    // Handle right mouse
    int current_r_mouse_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
    if (current_r_mouse_state == GLFW_PRESS) {
        double xposd, yposd;
        float xpos, ypos;
        glfwGetCursorPos(window, &xposd, &yposd);
        xpos = float(xposd);
        ypos = float(SCR_HEIGHT - yposd);
        if (r_mouse_state_ == GLFW_RELEASE) {
            RightMouseClick(xpos, ypos);
        }
        if (r_mouse_state_ == GLFW_PRESS &&
            (std::abs(xpos - last_cursor_x_) > 2.f || std::abs(ypos - last_cursor_y_) > 2.f)) {
            RightMouseMove(last_cursor_x_, last_cursor_y_, xpos, ypos);
        }
        last_cursor_x_ = float(xpos);
        last_cursor_y_ = float(ypos);
    }
    if (current_r_mouse_state == GLFW_RELEASE) {
        if (r_mouse_state_ == GLFW_PRESS) {
            RightMouseRelease();
        }
    }
    r_mouse_state_ = current_r_mouse_state;
}

void MainFrame::LeftMouseClick(float x, float y) {
    if (modeling_state_ == OBJ_SUBDIVIDE) {
        glm::vec3 p_world = Screen2World(x, y);
        glm::vec3 cam_pos = camera_.view_mat_inv_ * glm::vec4(0.f, 0.f, 0.f, 1.f);
        mesh_.SubdivideFace(cam_pos, glm::normalize(p_world - cam_pos));
    } else if (modeling_state_ == OBJ_EXTRUDE) {
        glm::vec3 p_world = Screen2World(x, y);
        glm::vec3 cam_pos = camera_.view_mat_inv_ * glm::vec4(0.f, 0.f, 0.f, 1.f);
        mesh_.GenExtrudeFace(cam_pos, glm::normalize(p_world - cam_pos));
    }
}

void MainFrame::LeftMouseRelease() {
    mesh_.CommitTransform();
}

void MainFrame::RightMouseClick(float x, float y) {
    return;
}

void MainFrame::RightMouseMove(float start_x, float start_y, float curr_x, float curr_y) {
    glm::vec2 s_start(start_x, start_y);
    glm::vec2 s_cur(curr_x, curr_y);
    glm::vec2 V = s_cur - s_start;
    glm::vec2 A = glm::vec2(-V.y, V.x);
    glm::vec3 rot_axis = glm::normalize(Screen2World(A + s_start) - Screen2World(s_start));
    glm::mat4x4 rot_mat = glm::rotate(glm::mat4x4(1.f), 0.007f * glm::length(A), rot_axis);
    camera_.ApplyTransform(rot_mat);
}

void MainFrame::RightMouseRelease() {
    return;
}

glm::vec3 MainFrame::Camera2World(const glm::vec3 &x, float w) {
    return glm::vec3(camera_.view_mat_inv_ * glm::vec4(x, w));
}

glm::vec3 MainFrame::World2Camera(const glm::vec3 &x, float w) {
    return glm::vec3(camera_.view_mat_ * glm::vec4(x, w));
}

glm::vec3 MainFrame::Screen2World(const glm::vec2 &v, float depth) {
    float x = v.x / SCR_WIDTH * 2.f - 1.f;
    float y = v.y / SCR_HEIGHT * 2.f - 1.f;
    float focal = std::tan(camera_.fov_ * .5f / 180.f * glm::pi<float>());
    glm::vec4 v_camera(x * focal * aspect, y * focal, -1.f, 1.f);
    v_camera = v_camera * depth;
    glm::vec4 v_world = camera_.view_mat_inv_ * v_camera;
    return glm::vec3(v_world);
}

glm::vec3 MainFrame::Screen2World(float scr_x, float scr_y, float camera_z) {
    float x = scr_x / SCR_WIDTH * 2.f - 1.f;
    float y = scr_y / SCR_HEIGHT * 2.f - 1.f;
    float focal = std::tan(camera_.fov_ * .5f / 180.f * glm::pi<float>());
    glm::vec4 v_camera(x * focal * aspect, y * focal, -1.f, 1.f);
    v_camera = v_camera * -camera_z;
    glm::vec4 v_world = camera_.view_mat_inv_ * v_camera;
    return glm::vec3(v_world);
}

std::tuple<glm::vec3, glm::vec3> MainFrame::Screen2WorldRay(float scr_x, float scr_y) {
    float x = scr_x / SCR_WIDTH * 2.f - 1.f;
    float y = scr_y / SCR_HEIGHT * 2.f - 1.f;
    float focal = std::tan(camera_.fov_ * .5f / 180.f * glm::pi<float>());
    glm::vec3 o = camera_.view_mat_inv_ * glm::vec4(0.f, 0.f, 0.f, 1.f);
    glm::vec4 v_camera(x * focal * aspect, y * focal, -1.f, 0.f);
    glm::vec3 v = camera_.view_mat_inv_ * v_camera;
    return std::make_tuple(o, v);
}

// ===== SOLUTION HELPERS IMPLEMENTATION BEGIN =====
// Hint: These helpers wrap the typical math (camera axes, pivot composition). Delete the entire solution block
//       when preparing the template and leave the hints so students know what needs to be rebuilt.
glm::vec3 MainFrame::CameraRight() const {
    return glm::normalize(glm::vec3(camera_.view_mat_inv_[0]));
}

glm::vec3 MainFrame::CameraUp() const {
    return glm::normalize(glm::vec3(camera_.view_mat_inv_[1]));
}

glm::vec3 MainFrame::CameraForward() const {
    return glm::normalize(glm::vec3(camera_.view_mat_inv_[2]));
}

glm::vec3 MainFrame::ActiveMeshCenter() const {
    return mesh_.center_;
}

glm::mat4 MainFrame::ComposeAroundPivot(const glm::vec3 &pivot, const glm::mat4 &transform) const {
    glm::mat4 translate_to_origin = glm::translate(glm::mat4(1.f), -pivot);
    glm::mat4 translate_back = glm::translate(glm::mat4(1.f), pivot);
    return translate_back * transform * translate_to_origin;
}
// ===== SOLUTION HELPERS IMPLEMENTATION END =====

void MainFrame::DrawScene() {
    // Draw mesh
    mesh_.Draw();

    VisualizeWorldSpace();
}