#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform vec2  uResolution;
uniform vec3  uCamPos;
uniform vec3  uCamForward;
uniform vec3  uCamRight;
uniform vec3  uCamUp;
uniform float uFovY;
uniform float uTime;
uniform int   uShowDisk;
uniform int   uMetric;
uniform float uSpin;
uniform sampler2D uSkybox;
uniform sampler2D uDiskTex;
const float PI = 3.141592653589793;

const int   MAX_STEPS  = 400;
const int   KERR_STEPS = 500;
const float STEP_SCALE = 0.04;
const float STEP_MIN   = 0.005;
const float STEP_MAX   = 2.0;
const float HORIZON_R  = 1.001;
const float ESCAPE_R   = 40.0;

const float M            = 0.5;
const float DISK_INNER   = 1.50;
const float DISK_OUTER   = 12.0;
const float T_INNER      = 4000.0;

const float SWIRL_SPEED  = 1.0;
const float SWIRL_LOOP   = 12.0;
const float DISK_DENSITY = 1.5;

vec3 sampleSkybox(vec3 d) {
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;
    float v = asin(clamp(d.y, -1.0, 1.0)) / PI + 0.5;
    return texture(uSkybox, vec2(u, v)).rgb;
}

vec3 blackbodyRGB(float kelvin) {
    float t = clamp(kelvin, 1000.0, 40000.0) / 100.0;
    vec3 c;
    c.r = t <= 66.0 ? 1.0
                    : 1.29293618606 * pow(t - 60.0, -0.1332047592);
    c.g = t <= 66.0 ? 0.39008157876 * log(t) - 0.63184144378
                    : 1.12989086089 * pow(t - 60.0, -0.0755148492);
    c.b = t >= 66.0 ? 1.0
        : (t <= 19.0 ? 0.0 : 0.54320678911 * log(t - 10.0) - 1.19625408914);
    return clamp(c, 0.0, 1.0);
}

vec3 geodesicAccel(vec3 pos, float L2) {
    float r2 = dot(pos, pos);
    float r = sqrt(r2);
    float invR5 = 1.0 / (r2 * r2 * r);
    return (-1.5 * L2 * invR5) * pos;
}

void rk4Step(inout vec3 pos, inout vec3 vel, float L2, float h) {
    vec3 k1p = vel;
    vec3 k1v = geodesicAccel(pos, L2);

    vec3 k2p = vel + 0.5 * h * k1v;
    vec3 k2v = geodesicAccel(pos + 0.5 * h * k1p, L2);
    vec3 k3p = vel + 0.5 * h * k2v;
    vec3 k3v = geodesicAccel(pos + 0.5 * h * k2p, L2);

    vec3 k4p = vel + h * k3v;
    vec3 k4v = geodesicAccel(pos + h * k3p, L2);

    pos += (h / 6.0) * (k1p + 2.0 * k2p + 2.0 * k3p + k4p);
    vel += (h / 6.0) * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);
}

vec3 diskEmission(vec3 hit, vec3 rayVel, float rc) {
    float gGrav = sqrt(1.0 - 1.0 / rc);

    float speed = sqrt(M / (rc - 2.0 * M));
    vec3 phiHat = normalize(vec3(-hit.z, 0.0, hit.x));
    vec3 beta = speed * phiHat;
    vec3 toCamera = normalize(-rayVel);
    float gamma = inversesqrt(1.0 - speed * speed);
    float doppler = 1.0 / (gamma * (1.0 - dot(beta, toCamera)));

    float g = doppler * gGrav;
    float g2 = g * g;
    float brightness = g2 * g2;

    float temp = T_INNER * pow(DISK_INNER / rc, 0.75);
    float tObs = g * temp;

    vec3 emission = blackbodyRGB(tObs) * brightness;
    return min(emission, vec3(8.0));
}

float diskDensity(vec3 hit, float rc) {
    float phi   = atan(hit.z, hit.x);
    float omega = sqrt(M / (rc * rc * rc));
    float base  = phi / (2.0 * PI) + 0.5;
    float v     = (rc - DISK_INNER) / (DISK_OUTER - DISK_INNER);

    float phase = uTime * SWIRL_SPEED / SWIRL_LOOP;
    float g1 = fract(phase);
    float g2 = fract(phase + 0.5);
    float off1 = omega * SWIRL_LOOP * g1;
    float off2 = omega * SWIRL_LOOP * g2;
    float t1 = texture(uDiskTex, vec2(base - off1, v)).r;
    float t2 = texture(uDiskTex, vec2(base - off2, v)).r;
    float tex = mix(t1, t2, abs(2.0 * g1 - 1.0));
    float radial = smoothstep(DISK_INNER, DISK_INNER + 0.3, rc)
                 * (1.0 - smoothstep(DISK_OUTER - 1.5, DISK_OUTER, rc));
    return tex * radial;
}

vec3 trace(vec3 pos, vec3 dir) {
    vec3 vel = dir;
    vec3 cp = cross(pos, vel);
    float L2 = dot(cp, cp);

    vec3 color = vec3(0.0);
    float transmittance = 1.0;

    for (int i = 0; i < MAX_STEPS; ++i) {
        float r = length(pos);
        if (r < HORIZON_R) return color;
        if (r > ESCAPE_R)  return color + transmittance * sampleSkybox(normalize(vel));

        float photonProx = exp(-3.0 * abs(r - 1.5) / 1.5);
        float localScale = mix(STEP_SCALE, STEP_SCALE * 0.25, photonProx);
        float h = clamp(localScale * r, STEP_MIN, STEP_MAX);
        vec3 prevPos = pos;
        rk4Step(pos, vel, L2, h);

        if (uShowDisk == 1 && prevPos.y * pos.y < 0.0) {
            float tCross = prevPos.y / (prevPos.y - pos.y);
            vec3 hit = mix(prevPos, pos, tCross);
            float rc = length(hit);
            if (rc >= DISK_INNER && rc <= DISK_OUTER) {
                float density = diskDensity(hit, rc);
                float alpha = clamp(density * DISK_DENSITY, 0.0, 1.0);
                color += transmittance * alpha * diskEmission(hit, vel, rc);
                transmittance *= (1.0 - alpha);
            }
        }
    }
    return color;
}

float kerrHorizon(float a) {
    return M + sqrt(max(M * M - a * a, 0.0));
}

float kerrRadius(vec3 X, float a) {
    float a2 = a * a;
    float d  = dot(X, X) - a2;
    return sqrt(max(0.5 * (d + sqrt(d * d + 4.0 * a2 * X.y * X.y)), 1e-12));
}

void kerrField(vec3 X, float a, out float r, out float f, out vec3 l) {
    float a2  = a * a;
    r = kerrRadius(X, a);
    float r2 = r * r;
    float Q   = r2 + a2;
    float KSF = r2 * r2 + a2 * X.y * X.y;
    l = vec3((r * X.x + a * X.z) / Q, X.y / r, (r * X.z - a * X.x) / Q);
    f = 2.0 * M * r * r2 / KSF;
}

void kerrDeriv(vec3 X, vec3 p, float E, float a, out vec3 dX, out vec3 dp) {
    float a2  = a * a;
    float r   = kerrRadius(X, a);
    float r2  = r * r;
    float r4  = r2 * r2;
    float Q   = r2 + a2;
    float Q2  = Q * Q;
    float KSF = r4 + a2 * X.y * X.y;
    vec3  l = vec3((r * X.x + a * X.z) / Q, X.y / r, (r * X.z - a * X.x) / Q);
    float f = 2.0 * M * r * r2 / KSF;
    vec3 gr = vec3(r * r2 * X.x, r * X.y * Q, r * r2 * X.z) / KSF;
    vec3 df = (2.0 * M / (KSF * KSF))
            * (r2 * (3.0 * a2 * X.y * X.y - r4) * gr
               - vec3(0.0, 2.0 * a2 * X.y * r2 * r, 0.0));
    vec3  dQ   = 2.0 * r * gr;
    float numx = r * X.x + a * X.z;
    float numz = r * X.z - a * X.x;
    float invr2 = 1.0 / r2;
    float dlpx = (((gr.x * X.x + r) * Q - numx * dQ.x) / Q2) * p.x
               + ((-X.y * gr.x) * invr2)                      * p.y
               + (((gr.x * X.z - a) * Q - numz * dQ.x) / Q2)  * p.z;
    float dlpy = (((gr.y * X.x)     * Q - numx * dQ.y) / Q2)  * p.x
               + ((r - X.y * gr.y) * invr2)                   * p.y
               + (((gr.y * X.z)     * Q - numz * dQ.y) / Q2)  * p.z;
    float dlpz = (((gr.z * X.x + a) * Q - numx * dQ.z) / Q2)  * p.x
               + ((-X.y * gr.z) * invr2)                      * p.y
               + (((gr.z * X.z + r) * Q - numz * dQ.z) / Q2)  * p.z;
    vec3 dlp = vec3(dlpx, dlpy, dlpz);

    float Phi = E + dot(l, p);
    dX = p - f * Phi * l;
    dp = 0.5 * Phi * Phi * df + f * Phi * dlp;
}

void kerrInit(vec3 camPos, vec3 dir, float a, out vec3 p, out float E) {
    float r, f;
    vec3 l;
    kerrField(camPos, a, r, f, l);
    E = 1.0;
    float A   = dot(l, dir);
    float k   = E / sqrt(max(1.0 - f * (1.0 - A * A), 1e-9));
    float Phi = (E + k * A) / (1.0 - f);
    p = k * dir + f * Phi * l;
}

vec3 traceKerr(vec3 camPos, vec3 dir) {
    float a = uSpin;
    vec3  p; float E;
    kerrInit(camPos, dir, a, p, E);

    vec3  X  = camPos;
    float rH = kerrHorizon(a) * 1.001;
    vec3  color = vec3(0.0);
    float transmittance = 1.0;
    float photonApprox = M * (1.0 + cos(0.6667 * acos(clamp(-a / M, -1.0, 1.0))));

    for (int i = 0; i < KERR_STEPS; ++i) {
        float R = length(X);
        float rK = kerrRadius(X, a);
        if (rK < rH) return color;

        vec3 k1X, k1P;
        kerrDeriv(X, p, E, a, k1X, k1P);

        if (R > ESCAPE_R)
            return color + transmittance * sampleSkybox(normalize(k1X));

        float proxK = exp(-3.0 * abs(rK - photonApprox) / max(photonApprox, 0.3));
        float localScaleK = mix(STEP_SCALE, STEP_SCALE * 0.25, proxK);
        float h = clamp(localScaleK * R, STEP_MIN, STEP_MAX);
        vec3 prevX = X;

        vec3 k2X, k2P, k3X, k3P, k4X, k4P;
        kerrDeriv(X + 0.5 * h * k1X, p + 0.5 * h * k1P, E, a, k2X, k2P);
        kerrDeriv(X + 0.5 * h * k2X, p + 0.5 * h * k2P, E, a, k3X, k3P);
        kerrDeriv(X + h * k3X,       p + h * k3P,       E, a, k4X, k4P);
        X += (h / 6.0) * (k1X + 2.0 * k2X + 2.0 * k3X + k4X);
        p += (h / 6.0) * (k1P + 2.0 * k2P + 2.0 * k3P + k4P);

        if (uShowDisk == 1 && prevX.y * X.y < 0.0) {
            float t   = prevX.y / (prevX.y - X.y);
            vec3  hit = mix(prevX, X, t);
            float rc  = length(hit);
            if (rc >= DISK_INNER && rc <= DISK_OUTER) {
                float density = diskDensity(hit, rc);
                float alpha   = clamp(density * DISK_DENSITY, 0.0, 1.0);
                color += transmittance * alpha * diskEmission(hit, k4X, rc);
                transmittance *= (1.0 - alpha);
            }
        }
    }
    return color;
}

void main() {
    vec2 ndc = vUV * 2.0 - 1.0;
    ndc.x *= uResolution.x / uResolution.y;

    float tf = tan(0.5 * uFovY);
    vec3 dir = normalize(uCamForward
                       + ndc.x * tf * uCamRight
                       + ndc.y * tf * uCamUp);
    vec3 col = (uMetric == 1) ? traceKerr(uCamPos, dir)
                              : trace(uCamPos, dir);
    FragColor = vec4(col, 1.0);
}
