#ifdef __IMAGE_SUPPORT__

//http://www.thebigblob.com/gaussian-blur-using-opencl-and-the-built-in-images-textures/

//if accessing pixels outside of the image, return a border color (0.f in first channel)
__constant sampler_t dataSampler = CLK_ADDRESS_CLAMP;
__constant sampler_t hotspotsSampler = CLK_ADDRESS_NONE;

//images are always in __global address space, and access has to be limited to either read or write only
__kernel void heatmap(unsigned int nextRoundNumber,
    __read_only image2d_t hotspotsStart, __read_only image2d_t hotspotsEnd,
    __read_only image2d_t in, __write_only image2d_t out){

    __private const int2 pos = {get_global_id(0), get_global_id(1)};

    __private float sum = 0.f;
    for(int a = -1; a < 2; a++) {
        for(int b = -1; b < 2; b++) {
            sum += read_imagef(in, dataSampler, pos + (int2)(a,b)).x;
        }
    }

    //set hotspots for next round
    __private unsigned int start = read_imageui(hotspotsStart, hotspotsSampler, pos).x;
    __private unsigned int end = read_imageui(hotspotsEnd, hotspotsSampler, pos).x;
    if (nextRoundNumber >= start && nextRoundNumber < end) {
        sum = 1.f;
    } else {
        sum = sum / 9.f;
    }

    write_imagef(out, pos, (float4)(sum, 0., 0., 0.));
}

#endif
