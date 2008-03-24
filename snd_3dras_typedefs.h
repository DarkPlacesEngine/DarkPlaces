// This file defines a few "basis measurement" types that extern program will need.
#ifndef Typedefs_h
#define Typedefs_h
#include <stdint.h>
                                    ///To address a location in a file.
typedef unsigned int                FilePosition;
                                    ///This will express an Amount of something.
typedef uint64_t                    Amount;
                                    /// Index expresses an address in a certain array of data.
typedef uint64_t                    Index;
                                    /// A signed index, to access things that can be access before 0
typedef  int64_t                    SignedIndex;
                                    ///The depth at witch we are tracing.
typedef unsigned int                TraceDepth;
                                    ///The type of a Location (as in a messurement ... from 0)
typedef  int64_t                    Location;
                                    ///The type of a Location on a texture
typedef float                       TextureLocation;
                                    ///The type of a Distance
typedef float                       Distance;
                                    ///The type of a Scalar type for use in: Normal3D, Dot product, Direction3D, ...
typedef float                       Scalar;
                                    ///Howmuch of something ?
typedef float                       Ratio;
                                    ///The type of a an EulerAngle for use in: EulerAngle2D, EulerAngle3D, ...
typedef float                       EulerAngle;
                                    ///The type that detemens the size of 1 Location. Expressed in meters/Location.
typedef float                       Scale;
                                    ///The frequency of something.
typedef float                       Frequency;
                                    ///The wavelength of a frequency
typedef Distance                    WaveLength;
                                    /// Howmany samples we take per secod
typedef float                       SampleRate;
                                    /// The type in witch we will express a SoundSample.
typedef float                       Sample;
                                    /// The type that express the speed of sound (meter/second).
typedef float                       SoundSpeed;
                                    /// The type that that express 1 Time. As in a small step. Note in the feature this will be a class. To make it ring.
typedef unsigned int                Time;
typedef float                       Duration;
//typedef StrongType <unsigned int>   Time;  // Example of a strong typecheck
                                    /// The amplitude of the SoundPower. This for export to an AudioOutputDevice.
typedef float                       SoundVolume;
                                    /// How mutch power per square meter is received per meter (Watt/Meter^2)
typedef float                       SoundIntensity;
                                    /// An expression of the power of sound source (Watt)
typedef float                       SoundPower; // W, The power of the sound source

typedef float                       LightIntensity;
typedef float                       LightPower;
typedef float                       Brightness;
typedef float                       Gamma;
typedef float                       Color;
typedef float                       RefractionIndex;
typedef unsigned int                Resolution;
#endif
