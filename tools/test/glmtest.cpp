
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <cstring>

std::string ToString(const glm::vec4& vec)
{
    char amen[512] = {0};
    std::snprintf(amen, sizeof(amen), "%.2f %.2f %.2f %.2f",
                  vec.x, vec.y, vec.z, vec.w);
    return amen;
}

void Print( glm::mat4 mat)
{
    // transpose because printing the matrix
    // by vectors prints it as row matrix instead
    // of a column matrix.
    mat = glm::transpose(mat);

    std::cout << "\n";
    std::cout << ToString(mat[0]) << "\n";
    std::cout << ToString(mat[1]) << "\n";
    std::cout << ToString(mat[2]) << "\n";
    std::cout << ToString(mat[3]) << "\n";
    std::cout << "\n";
}

void change_of_basis()
{
    // change of basis matrix from A to B
    glm::mat4 A_to_B;
    A_to_B[0] = glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f);
    A_to_B[1] = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
    A_to_B[2] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    A_to_B[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    glm::mat4 B_to_A = glm::inverse(A_to_B);

    // vector V relative to the original basis A
    const auto V_a = glm::vec4(40.0f, 20.0f, 10.0f, 0.0f);
    // vector V expressed relative to the basis B
    const auto V_b = A_to_B * V_a;

    std::cout << "v " << ToString(V_a) << std::endl;
    std::cout << "y " << ToString(V_b) << std::endl;

    std::cout << "Transformation relative to basis A\n";
    {
        glm::mat4 A_t = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        glm::mat4 A_t_in_B = A_to_B * A_t * B_to_A; //glm::inverse(A_to_B);

        std::cout << ToString(A_t * V_a) << std::endl;
        std::cout << ToString(B_to_A * A_t_in_B * V_b) << std::endl;
    }

    std::cout << "Transformation relative to basis B\n";
    {
        glm::mat4 B_t = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        glm::mat4 B_t_in_A = B_to_A * B_t * A_to_B;

        std::cout << ToString(B_t * V_b) << std::endl;
        std::cout << ToString(A_to_B * B_t_in_A * V_a) << std::endl;

    }
}

void matrix_inverse()
{

    // https://www.geeksforgeeks.org/check-if-a-matrix-is-invertible/
    //
    // In linear algebra, an n-by-n square matrix A is called Invertible,
    // if there exists an n-by-n square matrix B such that AB=BA=I
    // where ‘I‘ denotes the n-by-n identity matrix. The matrix B is
    // called the inverse matrix of A.
    //
    // A square matrix is Invertible if and only if its determinant is non-zero.

    // is a projection matrix invertible?
    const auto width  = 1024.0f;
    const auto height = 768.0f;
    const auto fov   = glm::radians(45.0f);
    const auto znear = 0.01f;
    const auto zfar  = 100.0f;
    const auto aspect =  width / height;

    // setup a GLM projection matrix.
    const auto& proj = glm::perspective(fov, aspect, znear, zfar);
    const auto& det = glm::determinant(proj);
    std::cout << "determinant: " << det << std::endl;
}

void perspective_projection()
{
    // in OpenGL application the typical transformation sequence
    // for vertex data is as follows.

    // 1. The model local vertices are transformed by the model_to_world (model)
    //    matrix which transforms the vertices into the "world" space.
    // 2. The world space vertices are transformed by the world_to_view (camera, view, eye)
    //    matrix which transforms the vertices into the view space, i.e. relative
    //    to the camera. If we define the camera with a position and some look at vector
    //    and use this data to create a camera transformation matrix then the actual
    //    world_to_view matrix is the inverse of this matrix.
    // 3. The vertices are then transformed by the projection matrix which transforms
    //    them into the clip space. This will flip the Z axis thus producing a vector in
    //    left handed coordinate space.
    //
    //    OpenGL fixed/built-in functionality follows:
    //
    // 4. The Clip coordinates are divided by the W component. This will normalize the
    //    vectors and transform the coordinates (vertices) into "NDC space" which is a
    //    normalized left handed coordinate system where X points to the right
    //    Y points up and Z points into depth. Positive z=1.0 will map to a depth buffer
    //    value that is greater than z=-1.0 which is less.  (see glDepthRange)
    // 5. Finally the NDC values are mapped (through viewport setting) to pixels (fragments)
    //    and depth values. (glViewport, glDepthRange)

    // When the projection matrix (+ division by W) transforms the scene into a unit cube,
    // the vertices that are at the corners of the far plane (intersecting with the corner
    // rays of the view frustum) get pulled "in" towards the center of projection. This
    // produces the perspective illusion of having a vanishing point.
    //
    // The key points of this transformation are the following:
    //    * It transforms a frustum that is oriented into the direction of the negative z axis into a unit cube.
    //    * It flips the z axis, which corresponds to changing the handedness of the coordinate system.

    const auto width  = 1024.0f;
    const auto height = 768.0f;
    const auto fov   = glm::radians(45.0f);
    const auto znear = 1.0f;
    const auto zfar  = 100.0f;
    const auto aspect =  width / height;

    // setup a GLM projection matrix.
    // remember the near and far planes are given by a *distance* from the viewer
    // not by an absolute Z axis value.
    const auto& proj = glm::perspective(fov, aspect, znear, zfar);
    const auto& inv  = glm::inverse(proj);

    {
        // geometry reminder
        //
        //           |
        //           |  y
        // _a)_______|
        //      x
        // tan(a) = y / x
        //   => a = atan(y / x)
        //   => y = x * tan(a)
        //   => x = y / tan(a)
        //
        constexpr auto a = glm::radians(49.0f);
        constexpr auto x = 17.0f;
        constexpr auto y = 19.56f;
        //std::cout << "X=" << y / tan(a) << std::endl;
        //std::cout << "Y=" << tan(a) * x << std::endl;
        //std::cout << "a=" << glm::degrees(atan(y/x)) << std::endl;
    }

    std::cout << "Perspective projection.\n";
    std::cout << "=======================\n";

    struct Point {
        const char* name;
        glm::vec4 point;
    };
    Point points[] = {
       "Center of far plane",      {0.0f, 0.0f, -100.0f, 1.0f},
       "Center top at far plane",  {0.0f,  tan(fov*0.5)*zfar, -zfar, 1.0f},
       "Center bot at far plane",  {0.0f, -tan(fov*0.5)*zfar, -zfar, 1.0f},
       "Center top at near plane", {0.0f,  tan(fov*0.5)*znear, -znear, 1.0f},
       "Center bot at near plane", {0.0f, -tan(fov*0.5)*znear, -znear, 1.0f},
    };
    for (const auto& p : points)
    {
        const auto& p_ = proj * p.point;
        std::cout << p.name << std::endl;
        std::cout << "World: " << ToString(p.point)    << std::endl;
        std::cout << "Clip:  " << ToString(p_)         << std::endl;
        std::cout << "NDC:   " << ToString(p_ / p_.w)  << std::endl;
        std::cout << std::endl;
    }
}

void orthographic_projection()
{
    const auto left   = 0.0f;
    const auto top    = 0.0f;
    const auto right  = 1000.0f;
    const auto bottom = 1000.0f;
    // typical (?) 2D projection, mapping window coordinates (X, Y) to cubic view volume.
    //
    // 0,0
    //   _________
    //  |         |
    //  |         |
    //  |_________|, 1000,1000
    //

    const auto near = 1.0f;
    const auto far  = 100.0f;
    // the near and far values are again (similar to projection) the *distances*
    // from the viewer to the near and far planes. (according to the original glOrtho
    // also negative values could be used in which case the planes are behind the
    // viewer. not sure what this means though...)
    const auto& proj = glm::ortho(left, right, bottom, top, near, far);

    std::cout << "Top Left Origin Orthographic Projection.\n";
    std::cout << "========================================\n";

    struct Point {
        const char* name;
        glm::vec4 point;
    };
    Point points[] = {
        {"Top left at near plane", {0.0f,   0.0f,   -1.0f,   1.0f}},
        {"Top left at far plane",  {0.0f,   0.0f,   -100.0f, 1.0f}},
        {"Center at near plane",   {500.0f, 500.0f, -1.0f,   1.0f}},
        {"Center at far plane",    {500.0f, 500.0f, -100.0f, 1.0f}}
    };
    for (const auto& p : points)
    {
        const auto& p_ = proj * p.point;
        std::cout << p.name << std::endl;
        std::cout << "World: " << ToString(p.point)    << std::endl;
        std::cout << "Clip:  " << ToString(p_)         << std::endl;
        std::cout << "NDC:   " << ToString(p_ / p_.w)  << std::endl;
        std::cout << std::endl;
    }
}

int main()
{
    // simple test to check that the matrix print
    // prints the correct output
    {
        const glm::mat4 test = {
            {1.0, 1.0, 1.0, 1.0}, // x vector
            {2.0, 2.0, 2.0, 2.0}, // y vector
            {3.0, 3.0, 3.0, 3.0}, // z vector
            {4.0, 4.0, 4.0, 4.0} // w vector
        };
        //Print(test);
    }

    // change_of_basis();
    // matrix_inverse();
    perspective_projection();
    orthographic_projection();
    return 0;
}