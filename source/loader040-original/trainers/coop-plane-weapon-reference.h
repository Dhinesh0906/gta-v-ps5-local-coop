/* Model parameters extracted from pinned freemode script reference. */
#pragma once
typedef struct {u32 model;float bomb_t,flare_y,flare_z,outer,inner,target_y;u32 flares;} PlaneWeaponModel;
static const PlaneWeaponModel plane_weapon_models[]={
    {3929093893u,0.4f,1.0f,0.0f,3.0f,1.5f,-4.0f,1u}, /* alkonost */
    {2771347558u,-1.0f,3.0f,-0.3f,3.0f,1.5f,-4.0f,1u}, /* alphaz1 */
    {2176659152u,0.36f,3.0f,2.0f,4.0f,3.0f,-4.0f,1u}, /* avenger */
    {3868033424u,0.36f,3.0f,2.0f,4.0f,3.0f,-4.0f,1u}, /* avenger3 */
    {4262088844u,0.43f,3.0f,2.0f,3.0f,1.5f,-4.0f,1u}, /* bombushka */
    {3650256867u,0.5f,3.0f,-0.3f,3.0f,1.5f,-4.0f,1u}, /* cuban800 */
    {84351789u,-1.0f,0.0f,-0.3f,3.0f,1.5f,-4.0f,1u}, /* duster2 */
    {3287439187u,-1.0f,3.0f,-0.3f,3.0f,1.5f,-4.0f,1u}, /* howard */
    {2531412055u,-1.0f,1.0f,3.0f,3.0f,1.5f,-4.0f,1u}, /* microlight */
    {3545667823u,0.45f,3.0f,-0.3f,3.0f,1.5f,-4.0f,1u}, /* mogul */
    {1565978651u,-1.0f,0.0f,-0.3f,3.0f,1.5f,-4.0f,1u}, /* molotok */
    {1036591958u,-1.0f,-0.8f,-0.3f,3.0f,1.5f,-4.0f,1u}, /* nokota */
    {2908775872u,-1.0f,-0.8f,-0.3f,3.0f,1.5f,-4.0f,1u}, /* pyro */
    {3319621991u,0.46f,0.0f,-0.3f,3.0f,1.5f,-4.0f,1u}, /* rogue */
    {3902291871u,0.5f,1.0f,-0.3f,3.0f,1.5f,-4.0f,1u}, /* seabreeze */
    {2594093022u,0.55f,0.0f,-0.3f,3.0f,1.5f,-4.0f,1u}, /* starling */
    {191916658u,0.4f,0.0f,0.0f,3.0f,1.5f,-4.0f,0u}, /* streamer216 */
    {1692272545u,0.7f,0.0f,0.0f,3.0f,1.5f,-4.0f,0u}, /* strikeforce */
    {858355070u,0.43f,-4.66f,-0.95f,3.0f,1.5f,-10.0f,1u}, /* titan2 */
    {1043222410u,0.6f,3.0f,2.0f,3.0f,1.5f,-4.0f,1u}, /* tula */
    {447548909u,0.54f,3.0f,0.0f,4.0f,3.0f,-4.0f,1u}, /* volatol */
};
