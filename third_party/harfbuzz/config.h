// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifdef HAVE_STUB_GETENV
    // Stub out getenv as not all platforms support it
    #define getenv(name) 0
#endif // HAVE_STUB_GETENV
