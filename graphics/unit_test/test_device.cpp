class TestShader : public gfx::Shader
{
public:
    explicit TestShader(std::string source) noexcept
      : mSource(std::move(source))
    {}

    virtual bool IsValid() const override
    { return true; }

    std::string GetFilename() const
    { return mFilename; }

    std::string GetSource() const
    { return mSource; }
private:
    std::string mFilename;
    std::string mSource;
};

class TestTexture : public gfx::Texture
{
public:
    void SetFlag(Flags flag, bool on_off) override
    {}
    void SetFilter(MinFilter filter) override
    { mMinFilter = filter; }
    void SetFilter(MagFilter filter) override
    { mMagFilter = filter; }
    MinFilter GetMinFilter() const override
    { return mMinFilter; }
    MagFilter GetMagFilter() const override
    { return mMagFilter; }
    void SetWrapX(Wrapping w) override
    { mWrapX = w; }
    void SetWrapY(Wrapping w) override
    { mWrapY = w; }
    Wrapping GetWrapX() const override
    { return mWrapX; }
    Wrapping GetWrapY() const override
    { return mWrapY; }
    void Upload(const void* bytes, unsigned xres, unsigned yres, Format format) override
    {
        mWidth  = xres;
        mHeight = yres;
        mFormat = format;
    }
    void Allocate(unsigned width, unsigned height, Format format) override
    {
        mWidth = width;
        mHash = height;
        mFormat = format;
    }
    void AllocateArray(unsigned width, unsigned height, unsigned array_size, Format format) override
    {

    }
    unsigned GetWidth() const override
    { return mWidth; }
    unsigned GetHeight() const override
    { return mHeight; }
    unsigned GetArraySize() const override
    { return 0; }
    Format GetFormat() const override
    { return mFormat; }
    void SetContentHash(size_t hash) override
    { mHash = hash; }
    size_t GetContentHash() const override
    { return mHash; }
    void SetName(const std::string&) override
    {}
    void SetGroup(const std::string&) override
    {}
    bool TestFlag(Flags flag) const override
    { return false; }

    bool GenerateMips() override
    {
        return false;
    }
    bool HasMips() const override
    {
        return false;
    }
    std::string GetName() const override
    { return ""; }
    std::string GetGroup() const override
    { return ""; }
    std::string GetId() const override
    { return "";}
private:
    unsigned mWidth  = 0;
    unsigned mHeight = 0;
    Format mFormat  = Format::AlphaMask;
    Wrapping mWrapX = Wrapping::Repeat;
    Wrapping mWrapY = Wrapping::Repeat;
    MinFilter mMinFilter = MinFilter::Default;
    MagFilter mMagFilter = MagFilter::Default;
    std::size_t mHash = 0;
};

class TestProgram : public gfx::Program
{
public:
    bool IsValid() const override
    { return true; }
    std::string GetName() const override
    { return {}; }
    std::string GetId() const override
    { return {}; }
private:

};


class TestGeometry : public gfx::Geometry
{
public:
    std::string GetName() const override
    { return ""; }
    Usage GetUsage() const override
    { return Usage::Static; }
    size_t GetNumDrawCmds() const override
    { return 0; }
    size_t GetContentHash() const override
    { return 0; }
    DrawCommand GetDrawCmd(size_t index) const override
    { return {}; }
private:
};

class TestDevice : public gfx::Device
{
public:
    void ClearColor(const gfx::Color4f& color, gfx::Framebuffer*, ColorAttachment) const override
    {}
    void ClearStencil(int value, gfx::Framebuffer* fbo) const override
    {}
    void ClearDepth(float value, gfx::Framebuffer* fbo) const override
    {}
    void ClearColorDepth(const gfx::Color4f& color, float depth, gfx::Framebuffer* fbo, ColorAttachment) const override
    {}
    void ClearColorDepthStencil(const gfx::Color4f& color, float depth, int stencil, gfx::Framebuffer* fbo, ColorAttachment) const override
    {}

    void SetDefaultTextureFilter(MinFilter filter) override
    {}
    void SetDefaultTextureFilter(MagFilter filter) override
    {}

    // resource creation APIs
    gfx::ShaderPtr FindShader(const std::string& id) override
    {
        auto it = mShaderIndexMap.find(id);
        if (it == mShaderIndexMap.end())
            return nullptr;
        return mShaders[it->second];
    }
    gfx::ShaderPtr CreateShader(const std::string& id, const gfx::Shader::CreateArgs& args) override
    {
        const size_t index = mShaders.size();
        mShaders.emplace_back(new TestShader(args.source));
        mShaderIndexMap[id] = index;
        return mShaders.back();
    }
    gfx::ProgramPtr FindProgram(const std::string& id) override
    {
       auto it = mProgramIndexMap.find(id);
       if (it == mProgramIndexMap.end())
           return nullptr;
       return mPrograms[it->second];
    }
    gfx::ProgramPtr CreateProgram(const std::string& id, const gfx::Program::CreateArgs&) override
    {
       const size_t index = mPrograms.size();
       mPrograms.emplace_back(new TestProgram);
       mProgramIndexMap[id] = index;
       return mPrograms.back();
    }
    gfx::GeometryPtr FindGeometry(const std::string& id) override
    {
        auto it = mGeomIndexMap.find(id);
        if (it == mGeomIndexMap.end())
            return nullptr;
        return mGeoms[it->second];
    }
    gfx::GeometryPtr CreateGeometry(const std::string& id, gfx::Geometry::CreateArgs args) override
    {
        const size_t index = mGeoms.size();
        mGeoms.emplace_back(new TestGeometry);
        mGeomIndexMap[id] = index;
        return mGeoms.back();
    }
    gfx::Texture* FindTexture(const std::string& name) override
    {
        auto it = mTextureIndexMap.find(name);
        if (it == mTextureIndexMap.end())
            return nullptr;
        return mTextures[it->second].get();
    }
    gfx::Texture* MakeTexture(const std::string& name) override
    {
        const size_t index = mTextures.size();
        mTextures.emplace_back(new TestTexture);
        mTextureIndexMap[name] = index;
        return mTextures.back().get();
    }
    gfx::Framebuffer* FindFramebuffer(const std::string& name) override
    {
        return nullptr;
    }
    gfx::Framebuffer* MakeFramebuffer(const std::string& name) override
    {
        return nullptr;
    }

    gfx::InstancedDrawPtr FindInstancedDraw(const std::string& id) override
    {
        return nullptr;
    }

    gfx::InstancedDrawPtr CreateInstancedDraw(const std::string& id, gfx::InstancedDraw::CreateArgs args) override
    {
        return nullptr;
    }

    const gfx::Texture* FindTexture(const std::string &name) const override
    {
        auto it = mTextureIndexMap.find(name);
        if (it == mTextureIndexMap.end())
            return nullptr;
        return mTextures[it->second].get();
    }

    // Resource deletion APIs
    void DeleteShaders() override
    {}
    void DeletePrograms() override
    {}
    void DeleteGeometries() override
    {}
    void DeleteTextures() override
    {}
    void DeleteTexture(const std::string& gpuId) override
    {

    }
    void DeleteFramebuffers() override
    {}
    void DeleteFramebuffer(const std::string&) override
    {}

    StateKey PushState() override
    {
        return 0;
    }
    void PopState(StateKey key) override
    {}

    void SetViewportState(const ViewportState&) const override
    {}
    void SetColorDepthStencilState(const ColorDepthStencilState&) const override
    {}
    void ModifyState(const StateValue&, StateName) const override
    {}

    void Draw(const gfx::Program& program, const gfx::ProgramState& program_state,
              const gfx::GeometryDrawCommand& geometry, const RasterState& state, gfx::Framebuffer* fbo) override
    {}

    void CleanGarbage(size_t, unsigned) override
    {}

    void BeginFrame() override
    {}
    void EndFrame(bool display) override
    {}
    gfx::Bitmap<gfx::Pixel_RGBA> ReadColorBuffer(unsigned width, unsigned height, gfx::Framebuffer* fbo) const override
    {
        gfx::Bitmap<gfx::Pixel_RGBA> bitmap;
        bitmap.Resize(width, height);
        bitmap.Fill(gfx::Color::DarkGreen);
        return bitmap;
    }
    gfx::Bitmap<gfx::Pixel_RGBA> ReadColorBuffer(unsigned x, unsigned y,
                                                 unsigned width, unsigned height, gfx::Framebuffer* fbo) const override
    {
        gfx::Bitmap<gfx::Pixel_RGBA> bitmap;
        bitmap.Resize(width, height);
        bitmap.Fill(gfx::Color::DarkGreen);
        return bitmap;
    }
    void GetResourceStats(ResourceStats* stats) const override
    {

    }
    void GetDeviceCaps(DeviceCaps* caps) const override
    {

    }
    const TestTexture& GetTexture(size_t index) const
    {
        TEST_REQUIRE(index < mTextures.size());
        return *mTextures[index].get();
    }

    const TestShader& GetShader(size_t index) const
    {
        TEST_REQUIRE(index < mShaders.size());
        return *mShaders[index].get();
    }

    void Clear()
    {
        mTextureIndexMap.clear();
        mTextures.clear();
        mShaderIndexMap.clear();
        mShaders.clear();
    }
    size_t GetNumTextures() const
    { return mTextures.size(); }
    size_t GetNumShaders() const
    { return mShaders.size(); }
    size_t GetNumPrograms() const
    { return mPrograms.size(); }

private:
    std::unordered_map<std::string, std::size_t> mTextureIndexMap;
    std::vector<std::unique_ptr<TestTexture>> mTextures;

    std::unordered_map<std::string, std::size_t> mGeomIndexMap;
    std::vector<std::shared_ptr<TestGeometry>> mGeoms;

    std::unordered_map<std::string, std::size_t> mShaderIndexMap;
    std::vector<std::shared_ptr<TestShader>> mShaders;

    std::unordered_map<std::string, std::size_t> mProgramIndexMap;
    std::vector<std::shared_ptr<TestProgram>> mPrograms;
};
