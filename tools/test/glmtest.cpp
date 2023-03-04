
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

int main()
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
    return 0;
}