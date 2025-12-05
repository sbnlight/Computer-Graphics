#include <iostream>
#include <vector>
#include <thread>
#include "Common.h"
#include "Scene.h"
#include "Camera.h"
#include "Material.h"
#include "Hittable.h"
#include "Utils/lodepng.h"

// TODO: feel free to modify the following parameters
const int kMaxTraceDepth = 5;
const int spp = 512;
const int NUM_THREAD = 8;
const std::string scene_file = "scene/teapot_area_light.toml";
const std::string output_path = "outputs/output.png";

Color TraceRay(const Ray &ray, const Hittable &hittable_collection,
               int trace_depth);

Color Shade(const Hittable &hittable_collection,
            const HitRecord &hit_record,
            int trace_depth);

void tracePixel(int x, int y, int width, int height, int thread_spp, const Camera &camera, const Scene &scene,
                float *image_buffer) {
    Color color(0.f, 0.f, 0.f);
    for (int i = 0; i < thread_spp; ++i) {
        float bias_x = get_random_float();
        float bias_y = get_random_float();
        Ray ray = camera.RayAt(float(x) + bias_x, float(y) + bias_y);
        color += TraceRay(ray, scene.hittable_collection_, 1);
    }
    color /= float(thread_spp);
    int idx = 3 * ((height - y - 1) * width + x);
    image_buffer[idx] += color.r;
    image_buffer[idx + 1] += color.g;
    image_buffer[idx + 2] += color.b;
}

void thread_func(int thread_i, int n_thread, int spp, int width, int height, const Camera &camera, const Scene &scene,
                 float *image_buffer) {
    auto now = std::chrono::high_resolution_clock::now();
    int thread_spp = spp;
    float progress = 0.f;

    for (int x = thread_i; x < width; x += n_thread) {
        for (int y = 0; y < height; ++y) {
            tracePixel(x, y, width, height, thread_spp, camera, scene, image_buffer);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - now;
    std::cout << "Thread " << thread_i + 1 << " finished in " << elapsed.count() << " seconds." << std::endl;
}

int main() {
    const std::string work_dir("D:/2025 sem1/comp3271/COMP3271_Computer_Graphics-main-20251204/COMP3271_Computer_Graphics-main/Assignment3/");

    // Construct scene
    Scene scene(work_dir, scene_file);
    const Camera &camera = scene.camera_;
    int width = camera.width_;
    int height = camera.height_;

    std::vector<unsigned char> image(width * height * 4, 0);
    float *image_buffer = new float[width * height * 3]();

    std::vector<std::thread> thread_pool;
    for (int thread_i = 0; thread_i < NUM_THREAD; ++thread_i) {
        thread_pool.emplace_back(thread_func, thread_i, NUM_THREAD, spp, width, height, std::ref(camera),
                                 std::ref(scene), image_buffer);
    }
    for (std::thread &t: thread_pool) {
        t.join();
    }

    // Copy from image_buffer to image
    for (int i = 0; i < width * height; ++i) {
        for (int j = 0; j < 3; ++j) {
            image[4 * i + j] = static_cast<uint8_t>(glm::min(image_buffer[3 * i + j], 1.f - 1e-5f) * 256.f);
        }
        image[4 * i + 3] = 255;
    }


    std::vector<unsigned char> png;
    unsigned error = lodepng::encode(png, image, width, height);
    lodepng::save_file(png, work_dir + output_path);
}

Vec SampleHemisphere(const Vec &normal) {

    auto ray = Vec(0.0f, 0.0f, 0.0f);
    // TODO 3: add your code here.
    // This function randomly samples a direction on the hemisphere.
    // Hint: sample a random direction on the hemisphere with normal as the z-axis
    // Hint: Use Rodrigues' rotation formula to rotate the z-axis to the normal direction
    
    // Uniform sampling on a hemisphere.
    
    // 1. Generate two random numbers
    float u1 = get_random_float();
    float u2 = get_random_float();

    // 2. Uniform sampling on the unit hemisphere (local coordinates)
    // PDF = 1 / (2 * PI)
    // cos(theta) = u1 for cosine weighted, but for uniform: z = u1 (or 1-u1)
    // To match typical spherical uniform sampling:
    // z = random([0, 1]), r = sqrt(1 - z^2), phi = 2*PI*random
    
    float z = u1; 
    float r = sqrt(glm::max(0.f, 1.0f - z * z));
    float phi = 2.0f * M_PI * u2;

    float x = r * cos(phi);
    float y = r * sin(phi);

    // Local direction
    Vec local_dir(x, y, z);

    // 3. Align local z-axis (0,0,1) with the surface normal (world coordinates)
    // Using an Orthonormal Basis (TBN)
    
    Vec n = glm::normalize(normal);
    // Find a helper vector that is not parallel to n
    Vec helper = (fabs(n.x) > 0.9f) ? Vec(0, 1, 0) : Vec(1, 0, 0);
    
    Vec tangent = glm::normalize(glm::cross(n, helper));
    Vec bitangent = glm::cross(n, tangent);

    // Transform local direction to world space
    Vec world_dir = local_dir.x * tangent + local_dir.y * bitangent + local_dir.z * n;

    return glm::normalize(world_dir);
}

Vec samplePhongLobe(const Vec &reflectionDir, double shininess) {
    float u1 = get_random_float();
    float u2 = get_random_float();

    float phi = 2 * M_PI * u1;
    float cosTheta = pow(u2, 1.0 / (shininess + 1));
    float sinTheta = sqrt(1 - cosTheta * cosTheta);

    float x = sinTheta * cos(phi);
    float y = sinTheta * sin(phi);
    float z = cosTheta;

    // Build orthonormal basis around reflectionDir
    Vec w = reflectionDir; // Note: reflectionDir should be normalized
    Vec up = fabs(w.z) < 0.999 ? Vec(0, 0, 1) : Vec(1, 0, 0);
    Vec u = glm::normalize(glm::cross(up, w));
    Vec v = glm::cross(w, u);

    // Convert to world coordinates
    Vec newDir = glm::normalize(u * x + v * y + w * z);
    return newDir;
}

Color Shade(const Hittable &hittable_collection,
            const HitRecord &hit_record,
            int trace_depth) {
    Color color(0.f, 0.f, 0.f);

    const Material &material = hit_record.material;
    Point o = hit_record.position;
    color = material.ambient * material.k_a;

    // 1. If the object has emission, add its contribution to the color.
    color += material.emission;

    // Bonus Task: Mirror Reflection
    // If shininess is negative, treat as a perfect mirror
    if (material.sh < 0.f && trace_depth < kMaxTraceDepth) {
        Vec reflect_dir = glm::normalize(hit_record.reflection);
        Vec origin_offset = hit_record.position + 1e-4f * hit_record.normal;
        Color reflected_color = TraceRay(Ray(origin_offset, reflect_dir), hittable_collection, trace_depth + 1);
        // Add reflected color (weighted by specular component/k_s)
        color += material.specular * material.k_s * reflected_color;
        
        // Mirror usually replaces diffuse/glossy, so we return here or use else/if logic.
        // Based on typical Phong mix, we might stop here for a perfect mirror surface.
        return color; 
    }

    if (material.k_d > 0.f && trace_depth < kMaxTraceDepth) {
        // TODO 4: implement the diffuse reflection part of the shader.
        
        // Monte Carlo Integration for Diffuse Indirect Lighting
        
        // Sample a random direction on the hemisphere
        Vec wi = SampleHemisphere(hit_record.normal);
        
        // Calculate Cosine term (Lambert's law)
        float cos_theta = glm::dot(wi, hit_record.normal);
        
        // Ensure the ray is in the upper hemisphere
        if (cos_theta > 0.0f) {
            // Probability Density Function for Uniform Hemisphere sampling
            float pdf = 1.0f / (2.0f * M_PI);
            
            // offset
            Vec origin_offset = hit_record.position + 1e-4f * hit_record.normal;
            Color Li = TraceRay(Ray(origin_offset, wi), hittable_collection, trace_depth + 1);
            // Rendering Equation Estimator: (BRDF * Li * cos_theta) / PDF
            // Diffuse BRDF = kd * diffuse_color / PI
            // Here material.diffuse acts as the color, k_d is the weight.
            // Note: In many simplified implementations (like this template implies), 
            // the 1/PI from BRDF cancels out with PI in integration or is handled differently.
            // However, strictly: Color += (k_d * diffuse / PI) * Li * cos_theta / (1/2PI)
            //                        = k_d * diffuse * Li * cos_theta * 2.0
            // BUT, strictly following the inputs:
            // Let's assume standard Monte Carlo estimator.
            
            color += (material.diffuse / M_PI) * material.k_d * Li * cos_theta / pdf;
        }
        // color += diffuseReflectance * BRDF * cos_theta / pdf;
    }
    // 3. calculate specular
    if (material.k_s > 0.f && trace_depth < kMaxTraceDepth) {
        // Glossy specular reflection using Phong BRDF
        Vec reflection = glm::normalize(glm::reflect(hit_record.in_direction, hit_record.normal));
        // Sample around the reflection direction
        Vec newDir = samplePhongLobe(reflection, material.sh);
        float cosAlpha = glm::dot(newDir, reflection);
        if (cosAlpha < 0) cosAlpha = 0;
        // BRDF and PDF for Phong reflection
        Vec brdf = material.specular * ((material.sh + 2) / (2 * M_PI)) * (float) pow(cosAlpha, material.sh);
        float pdf = ((material.sh + 1) / (2 * M_PI)) * pow(cosAlpha, material.sh);
        Vec origin_offset = hit_record.position + 1e-4f * hit_record.normal;
        Vec reflectColor = TraceRay(Ray(origin_offset, newDir), hittable_collection, trace_depth + 1);
        float cosine = glm::dot(hit_record.normal, hit_record.reflection);
        color += material.k_s * reflectColor * brdf / pdf * cosine;
    }

    return color;
}

Color TraceRay(const Ray &ray,
               const Hittable &hittable_collection,
               int trace_depth) {
    HitRecord record;
    // Initialize the color as black
    Color color(0.0f, 0.0f, 0.0f);
    bool hit = hittable_collection.Hit(ray, &record);
    
    if (hit) {
        color = Shade(hittable_collection, record, trace_depth);
    }
    return color;
}
