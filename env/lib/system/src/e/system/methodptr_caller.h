#pragma once
#include "marshaler.h"
#include "any.h"
namespace e
{
	namespace system
	{

#ifdef _M_IX86
		static void __declspec(naked) __fastcall _MethodPtrCaller_RawCallWithDynamicParam(void* pFunc/*ecx*/, void* pParam/*edx*/, size_t pParamSize)
		{
			__asm {
				mov eax, [esp+4]

				test eax,eax
				jz endPush
				continuToPush:
				add eax, -4
				push dword ptr[edx + eax]
					jz endPush
					jmp continuToPush
					endPush :

				call ecx
				ret 4
			}
		}
		static void __declspec(naked) __fastcall _MethodPtrCaller_RawCdcelCallWithDynamicParam(void* pFunc/*ecx*/, void* pParam/*edx*/, size_t pParamSize)
		{
			__asm {
				push esi
				mov esi, [esp + 4]
				test esi, esi
				jz endPush
				continuToPush :
				add esi, -4
					push dword ptr[edx + esi]
					jz endPush
					jmp continuToPush
					endPush :

				call ecx
					add esp, esi
					pop esi
					ret 4
			}
		}
#endif 

		enum class CallingConventions
		{
			StdCall,
			Cdcel
		};

		template<typename TResult, typename... TArgs>
		struct MethodPtrCaller
		{
		};
		template<typename TResult, typename... TArgs>
		struct MethodPtrCaller<TResult(TArgs...)>
		{
		private:

			template<typename T> struct ParamMarshalHelper
			{
			private:
				using TMarshal = valMarshaler<T>;
			public:
				using NativeType = typename TMarshal::NativeType;
			private:
				T& value;
				NativeType x;
			public:
				ParamMarshalHelper(T& _value) :value(_value)
				{
					TMarshal::marshal(x, value);
				}
				~ParamMarshalHelper()
				{
					TMarshal::cleanup(x, value);
				}
				NativeType& get()
				{
					return x;
				}
			};

#ifdef _M_X64
			template<> struct ParamMarshalHelper<e::system::any>
			{
			public:
				using NativeType = double;
			private:
				typename e::system::any& value;
				double x;
			public:
				ParamMarshalHelper(e::system::any& _value) :value(_value)
				{
					value.byVal_Marshal(reinterpret_cast<__int64 &>(x));
				}
				~ParamMarshalHelper()
				{
					value.byVal_Cleanup(reinterpret_cast<__int64 &>(x));
				}
				double& get()
				{
					return x;
				}
			};
#endif

			template<typename T> constexpr static auto isByValAnyArg_v = std::is_same_v<T, e::system::any>;
			constexpr static int getIndexOfFirstByValAnyParam()
			{
				constexpr bool x[] = { isByValAnyArg_v<TArgs>...};
				for (int i = 0; i < sizeof(x); i++)        
				{
					if (x[i]) return i;
				}
				return -1;
			};

		public:
			template <CallingConventions callingConvention = CallingConventions::StdCall>
			static TResult call(void* func, TArgs... args)
			{
				using FuncPtrType = std::conditional_t<callingConvention == CallingConventions::Cdcel, 
					typename resultReceiver<TResult>::NativeType (__cdecl *)(typename ParamMarshalHelper<TArgs>::NativeType...),
					typename resultReceiver<TResult>::NativeType (__stdcall *)(typename ParamMarshalHelper<TArgs>::NativeType...)> ;
				constexpr auto indexOfFirstAnyParam = getIndexOfFirstByValAnyParam();
				if constexpr (indexOfFirstAnyParam != -1)
				{
#ifdef _M_X64
					//�ο� https://docs.microsoft.com/zh-cn/cpp/build/x64-calling-convention#varargs
					//ʹ��Varargs��ʵ��ParamMarshalHelper<e::system::any>����double���ɱ�֤ ����Ĵ��� �� �����Ĵ��� ������ֵ

					//ע2��stdcall��cdcel��x64���ᱻ���ԣ���ʹ�ñ�׼x64����Լ��
					typename resultReceiver<TResult>::NativeType (*pFunc)(...);
					pFunc = static_cast<decltype(pFunc)>(func);
					if constexpr (std::is_void_v<TResult>)
					{
						return pFunc(ParamMarshalHelper<TArgs>(args).get()...);
					}
					else
					{
						return resultReceiver<TResult>::receive(pFunc(ParamMarshalHelper<TArgs>(args).get()...));
					}
#endif
#ifdef _M_IX86
					//�ο���
					//https://docs.microsoft.com/en-us/cpp/cpp/stdcall
					//https://docs.microsoft.com/en-us/cpp/cpp/calling-example-function-prototype-and-call
					//https://docs.microsoft.com/en-us/cpp/cpp/results-of-calling-example
					//stdcall��double, int64��QWORD������Ҫ8bytesջ�ռ䣨����ࣩ����������������ֻ��Ҫ4bytes�ռ䡣
					//����ζ���޷���x64�����ڱ���ʱȷ��ջ�ռ��С

					std::vector<intptr_t> paramVector; //ע���˴�Ϊ����������stdcall
					marshalToVector<TArgs...>(paramVector, args...);
					typename resultReceiver<TResult>::NativeType(__fastcall *pCallerFunc)(void*, void*, size_t);
					if constexpr (callingConvention == CallingConventions::Cdcel)
					{
						pCallerFunc = reinterpret_cast<decltype(pCallerFunc)>(_MethodPtrCaller_RawCdcelCallWithDynamicParam);
					}
					else
					{
						pCallerFunc = reinterpret_cast<decltype(pCallerFunc)>(_MethodPtrCaller_RawCallWithDynamicParam);
					}
					if constexpr (std::is_void_v<TResult>)
					{
						pCallerFunc(func, paramVector.data(), paramVector.size() * sizeof(intptr_t));
						cleanupFromVector<TArgs...>(paramVector, args...);
					}
					else
					{
						auto r = resultReceiver<TResult>::receive(pCallerFunc(func, paramVector.data(), paramVector.size() * sizeof(intptr_t)));
						cleanupFromVector<TArgs...>(paramVector, args...);
						return r;
					}
#endif 
				}
				else
				{
					FuncPtrType pFunc;
					pFunc = static_cast<decltype(pFunc)>(func);
					if constexpr (std::is_void_v<TResult>)
					{
						return pFunc(ParamMarshalHelper<TArgs>(args).get()...);
					}
					else
					{
						return resultReceiver<TResult>::receive(pFunc(ParamMarshalHelper<TArgs>(args).get()...));
					}
				}
			}
		};
	}
}