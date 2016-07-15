#include "Common/x64Emitter.h"
#include "VideoCommon/VertexLoaderBase.h"

class VertexLoaderX64 : public VertexLoaderBase, public Gen::X64CodeBlock
{
public:
	VertexLoaderX64(const TVtxDesc& vtx_desc, const VAT& vtx_att);

protected:
	bool IsInitialized() override
	{
		return true;
	}
	int RunVertices(const VertexLoaderParameters &parameters) override;
	bool EnvironmentIsSupported() override;
private:
	u32 m_src_ofs = 0;
	u32 m_dst_ofs = 0;
	Gen::FixupBranch m_skip_vertex;
	Gen::OpArg GetVertexAddr(int array, u64 attribute);
	int ReadVertex(Gen::OpArg data, u64 attribute, int format, int count_in, int count_out, bool dequantize, AttributeFormat* native_format, Gen::X64Reg scaling_register);
	void ReadColor(Gen::OpArg data, u64 attribute, int format);
	void GenerateVertexLoader();
};
