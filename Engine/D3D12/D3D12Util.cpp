#include "D3D12Util.h"
#include <comdef.h>

Microsoft::WRL::ComPtr<ID3DBlob> D3DUtil::LoadBinary(const std::wstring& filename)
{
}

Microsoft::WRL::ComPtr<ID3D12Resource> D3DUtil::CreateDefaultBuffer(ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList, const void* initData, UINT64 byteSize,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
}

Microsoft::WRL::ComPtr<ID3DBlob> D3DUtil::CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines,
	const std::string& entrypoint, const std::string& target)
{
}

DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
	ErrorCode(hr), FunctionName(functionName), Filename(filename), LineNumber(lineNumber)
{

}

std::wstring DxException::ToString() const
{
	// Get the string description of the error code.
	_com_error err(ErrorCode);
	std::wstring msg = err.ErrorMessage();

	return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
}
