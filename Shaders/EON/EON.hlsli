#ifndef EON_HLSLI
#define EON_HLSLI

/*
    Provides the source code listings given in the paper:

        https://arxiv.org/abs/2410.18026

        "EON: A practical energy-preserving rough diffuse BRDF",
            Jamie Portsmouth, Peter Kutz, Stephen Hill

    Note that this implementation assumes throughout that the directions are
    specified in a local space where the z-direction aligns with the surface normal.

    Translated from the paper's GLSL listing. HLSL float3x3 stores rows, while
    GLSL mat3 stores columns, so basis matrices are transposed on construction
    and vectors are multiplied with mul(matrix, vector).
*/

static const float pi            = 3.14159265f;
static const float rcppi         = 1.0f / pi;
static const float constant1_FON = 0.5f - 2.0f / (3.0f * pi);
static const float constant2_FON = 2.0f / 3.0f - 28.0f / (15.0f * pi);

// FON directional albedo (analytic)
float E_FON_exact(float mu, float r)
{
    float AF = 1.0f / (1.0f + constant1_FON * r); // FON A coeff.
    float BF = r * AF;                            // FON B coeff.
    mu = clamp(mu, -1.0f, 1.0f);
    float Si = sqrt(1.0f - (mu * mu));
    float G = Si * (acos(mu) - Si * mu)
            + (2.0f / 3.0f) * (Si * mu * (1.0f + Si + (Si * Si)) / (1.0f + Si) - Si);
    return AF + (BF * rcppi) * G;
}

// FON directional albedo (approx.)
float E_FON_approx(float mu, float r)
{
    float mucomp = 1.0f - mu;
    const float g1 = 0.0571085289f;
    const float g2 = 0.491881867f;
    const float g3 = -0.332181442f;
    const float g4 = 0.0714429953f;
    float GoverPi = mucomp * (g1 + mucomp * (g2 + mucomp * (g3 + mucomp * g4)));
    return (1.0f + r * GoverPi) / (1.0f + constant1_FON * r);
}

////////////////////////////////////////////////////////////////////////////////
// EON BRDF
////////////////////////////////////////////////////////////////////////////////

// Evaluates EON BRDF value, given inputs:
//          rho = single-scattering albedo parameter
//            r = roughness in [0, 1]
//     wi_local = direction of incident ray (directed away from vertex)
//     wo_local = direction of outgoing ray (directed away from vertex)
//        exact = flag to select exact or fast approx. version
float3 f_EON(float3 rho, float r, float3 wi_local, float3 wo_local, bool exact)
{
    float mu_i = wi_local.z;                               // input angle cos
    float mu_o = wo_local.z;                               // output angle cos
    float s = dot(wi_local, wo_local) - mu_i * mu_o;       // QON s term
    float sovertF = s > 0.0f ? s / max(mu_i, mu_o) : s;    // FON s/t
    float AF = 1.0f / (1.0f + constant1_FON * r);          // FON A coeff.
    float3 f_ss = (rho * rcppi) * AF * (1.0f + r * sovertF); // single-scatter lobe
    float EFo = exact ? E_FON_exact(mu_o, r) :             // FON wo albedo (exact)
                        E_FON_approx(mu_o, r);             // FON wo albedo (approx)
    float EFi = exact ? E_FON_exact(mu_i, r) :             // FON wi albedo (exact)
                        E_FON_approx(mu_i, r);             // FON wi albedo (approx)
    float avgEF = AF * (1.0f + constant2_FON * r);         // avg. albedo
    float3 rho_ms = (rho * rho) * avgEF / (float3(1.0f, 1.0f, 1.0f) - rho * (1.0f - avgEF));
    const float eps = 1.0e-7f;
    float3 f_ms = (rho_ms * rcppi) * max(eps, 1.0f - EFo)  // multi-scatter lobe
                                 * max(eps, 1.0f - EFi)
                                 / max(eps, 1.0f - avgEF);
    return f_ss + f_ms;
}

////////////////////////////////////////////////////////////////////////////////
// EON directional albedo
////////////////////////////////////////////////////////////////////////////////

float3 E_EON(float3 rho, float r, float3 wi_local, bool exact)
{
    float mu_i = wi_local.z;                       // input angle cos
    float AF = 1.0f / (1.0f + constant1_FON * r);  // FON A coeff.
    float EF = exact ? E_FON_exact(mu_i, r) :      // FON wi albedo (exact)
                       E_FON_approx(mu_i, r);      // FON wi albedo (approx)
    float avgEF = AF * (1.0f + constant2_FON * r); // average albedo
    float3 rho_ms = (rho * rho) * avgEF / (float3(1.0f, 1.0f, 1.0f) - rho * (1.0f - avgEF));
    return rho * EF + rho_ms * (1.0f - EF);
}

////////////////////////////////////////////////////////////////////////////////
// EON importance sampling
////////////////////////////////////////////////////////////////////////////////

// V is assumed to be in local (+Z) space.
// Returns the same transform as GLSL mat3(X, Y, Z), whose columns are the basis.
float3x3 orthonormal_basis_ltc(float3 V)
{
    float lenSqr = dot(V.xy, V.xy);
    float3 X = lenSqr > 0.0f ? float3(V.x, V.y, 0.0f) * rsqrt(lenSqr) : float3(1, 0, 0);
    float3 Y = float3(-X.y, X.x, 0.0f); // cross(Z, X)
    return transpose(float3x3(X, Y, float3(0, 0, 1)));
}

void ltc_coeffs(float mu, float r,
                out float a, out float b, out float c, out float d)
{
    a = 1.0 + r*(0.303392f + (-0.518982f + 0.111709f*mu)*mu + (-0.276266f + 0.335918f*mu)*r);
    b = r*(-1.16407f + 1.15859f*mu + (0.150815f - 0.150105f*mu)*r)/(mu*mu*mu - 1.43545f);
    c = 1.0f + r*(0.20013f + (-0.506373f + 0.261777f*mu)*mu);
    d = r*(0.540852f + (-1.01625f + 0.475392f*mu)*mu)/(-1.0743f + (0.0725628f + mu)*mu);
}

float4 cltc_sample(in float3 wo_local, float r, float u1, float u2)
{
    float a, b, c, d; ltc_coeffs(wo_local.z, r, a, b, c, d);   // coeffs of LTC M
    float R = sqrt(u1); float phi = 2.0f * pi * u2;            // CLTC sampling
    float x = R * cos(phi); float y = R * sin(phi);            // CLTC sampling
    float vz = 1.0f / sqrt(d*d + 1.0f);                        // CLTC sampling factors
    float s = 0.5f * (1.0f + vz);                              // CLTC sampling factors
    x = -lerp(sqrt(1.0f - y*y), x, s);                         // CLTC sampling
    float3 wh = float3(x, y, sqrt(max(1.0f - (x*x + y*y), 0.0f))); // wh sample via CLTC
    float pdf_wh = wh.z / (pi * s);                            // PDF of wh sample
    float3 wi = float3(a*wh.x + b*wh.z, c*wh.y, d*wh.x + wh.z);    // M wh (unnormalized)
    float len = length(wi);                                    // |M wh| = 1/|M^-1 wh|
    float detM = c*(a - b*d);                                  // |M|
    float pdf_wi = pdf_wh * len*len*len / detM;                // wi sample PDF
    float3x3 fromLTC = orthonormal_basis_ltc(wo_local);        // transform wi to world space
    wi = normalize(mul(fromLTC, wi));                          // transform wi to world space
    return float4(wi, pdf_wi);
}

float cltc_pdf(in float3 wo_local, in float3 wi_local, float r)
{
    float3x3 toLTC = transpose(orthonormal_basis_ltc(wo_local));                  // transform wi to LTC space
    float3 wi = mul(toLTC, wi_local);                                             // transform wi to LTC space
    float a, b, c, d; ltc_coeffs(wo_local.z, r, a, b, c, d);                      // coeffs of LTC M
    float detM = c*(a - b*d);                                                     // |M|
    float3 wh = float3(c*(wi.x - b*wi.z), (a - b*d)*wi.y, -c*(d*wi.x - a*wi.z)); // adj(M) wi
    float lensq = dot(wh, wh);                                                    // |M| |M^-1 wi|
    float vz = 1.0f / sqrt(d*d + 1.0f);                                           // CLTC sampling factors
    float s = 0.5f * (1.0f + vz);                                                 // CLTC sampling factors
    float pdf = detM*detM/(lensq*lensq) * max(wh.z, 0.0f) / (pi * s);             // wi sample PDF
    return pdf;
}

float3 uniform_lobe_sample(float u1, float u2)
{
    float sinTheta = sqrt(1.0f - u1*u1);
    float phi = 2.0f * pi * u2;
    return float3(sinTheta * cos(phi), sinTheta * sin(phi), u1);
}

float4 sample_EON(in float3 wo_local, float r, float u1, float u2)
{
    float mu = wo_local.z;
    float P_u = pow(r, 0.1f) * (0.162925f + (-0.372058f + (0.538233f - 0.290822f*mu)*mu)*mu);
    float P_c = 1.0f - P_u;
    float4 wi; float pdf_c;
    if (u1 <= P_u) {
        u1 = u1 / P_u;
        wi.xyz = uniform_lobe_sample(u1, u2);
        pdf_c = cltc_pdf(wo_local, wi.xyz, r); }
    else {
        u1 = (u1 - P_u) / P_c;
        wi = cltc_sample(wo_local, r, u1, u2);
        pdf_c = wi.w; }
    const float pdf_u = 1.0f / (2.0f * pi);
    wi.w = P_u*pdf_u + P_c*pdf_c;
    return wi;
}

float pdf_EON(in float3 wo_local, in float3 wi_local, float r)
{
    float mu = wo_local.z;
    float P_u = pow(r, 0.1f) * (0.162925f + (-0.372058f + (0.538233f - 0.290822f*mu)*mu)*mu);
    float P_c = 1.0 - P_u;
    float pdf_c = cltc_pdf(wo_local, wi_local, r);
    const float pdf_u = 1.0f / (2.0f * pi);
    return P_u*pdf_u + P_c*pdf_c;
}


////////////////////////////////////////////////////////////////////////////////
// Example usage in typical evaluate/sample/albedo routines for a renderer.
////////////////////////////////////////////////////////////////////////////////

/*
    Note that this implementation assumes throughout that the directions are
    specified in a local space where the z-direction aligns with the surface normal.

    We assume the usual convention for unidirectional path tracing,
    where the direction of the outgoing ray wo is known, and the
    incident ray direction wi is the one being sampled.

    Both the incident ray direction wi and the outgoing ray direction wo
    are oriented to point away from the surface, i.e., the incident ray is in the opposite direction
    to incident photons, while the outgoing ray is parallel to the outgoing photons.

    Also note that the sampling would work equally well for the reverse case,
    when tracing paths in the direction of light flow.
*/

//cbuffer EONExampleMaterial
//{
//    float3 albedo;
//    float roughness;
//};

static const float DENOM_TOLERANCE = 1.0e-7f;

//float3 diffuse_brdf_evaluate(in float3 wi_local, in float3 wo_local,
//                             inout float pdf_wo_local)
//{
//    if (wi_local.z < DENOM_TOLERANCE || wo_local.z < DENOM_TOLERANCE) return float3(0.0f, 0.0f, 0.0f);
//    pdf_wo_local = pdf_EON(wi_local, wo_local, roughness);
//    return f_EON(albedo, roughness, wi_local, wo_local, true);
//}
//
//float3 diffuse_brdf_sample(in float3 wo_local, inout uint rndSeed,
//                           out float3 wi_local, out float pdf_wi_local)
//{
//    if (wo_local.z < DENOM_TOLERANCE) return float3(0.0f, 0.0f, 0.0f);
//    float u1 = rand(rndSeed); float u2 = rand(rndSeed);
//    float4 wiP = sample_EON(wo_local, roughness, u1, u2);
//    wi_local     = wiP.xyz;
//    pdf_wi_local = wiP.w;
//    return f_EON(albedo, roughness, wi_local, wo_local, true);
//}
//
//float3 diffuse_brdf_albedo(in float3 wo_local, inout uint rndSeed)
//{
//    if (wo_local.z < DENOM_TOLERANCE) return float3(0.0f, 0.0f, 0.0f);
//    return E_EON(albedo, roughness, wo_local, true);
//}

#endif // EON_HLSLI
