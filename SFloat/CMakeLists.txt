nirvana_module (SFloat)

set (berkeley "berkeley-softfloat-3/source")
target_sources (SFloat PRIVATE
	module.cpp
	SFloat16.cpp
	state.cpp

	${berkeley}/s_normSubnormalF64Sig.c
	${berkeley}/s_countLeadingZeros64.c
	${berkeley}/s_normExtF80SigM.c
	${berkeley}/s_shiftRightM.c
	${berkeley}/s_shiftLeftM.c
 	${berkeley}/s_roundPackToF64.c
	${berkeley}/s_isNaNF128M.c
	${berkeley}/s_shiftNormSigF128M.c
	${berkeley}/s_shiftRightJam64.c
	${berkeley}/s_addCarryM.c
	${berkeley}/s_addComplCarryM.c
	${berkeley}/s_addF128M.c
	${berkeley}/s_addM.c
	${berkeley}/s_shiftRightJamM.c
	${berkeley}/s_subM.c
	${berkeley}/s_remStepMBy32.c
	${berkeley}/s_approxRecip32_1.c
	${berkeley}/s_compare128M.c
	${berkeley}/s_roundPackMToF128M.c
	${berkeley}/s_roundPackMToExtF80M.c
	${berkeley}/s_mul128MTo256M.c
	${berkeley}/s_invalidF128M.c
	${berkeley}/s_tryPropagateNaNF128M.c
	${berkeley}/s_shortShiftLeftM.c
	${berkeley}/s_shortShiftRightM.c
	${berkeley}/s_shortShiftRightJamM.c
	${berkeley}/s_normRoundPackMToF128M.c
	${berkeley}/s_approxRecip_1Ks.c

	${berkeley}/f64_to_f128M.c
	${berkeley}/extF80M_to_f128M.c
	${berkeley}/i32_to_f128M.c
	${berkeley}/ui32_to_f128M.c
	${berkeley}/i64_to_f128M.c
	${berkeley}/ui64_to_f128M.c
	${berkeley}/f128M_to_f64.c
	${berkeley}/f128M_to_extF80M.c
	${berkeley}/f128M_to_i32_r_minMag.c
	${berkeley}/f128M_to_i64_r_minMag.c
	${berkeley}/f128M_add.c
	${berkeley}/f128M_sub.c
	${berkeley}/f128M_mul.c
	${berkeley}/f128M_div.c
	${berkeley}/f128M_eq.c
	${berkeley}/f128M_le.c
	${berkeley}/f128M_lt.c

)	

target_compile_definitions (SFloat PRIVATE NIRVANA_NO_BAD_ALLOC)

if ((NIRVANA_TARGET_PLATFORM STREQUAL x86) OR (NIRVANA_TARGET_PLATFORM STREQUAL x64))

	target_sources (SFloat PRIVATE
		${berkeley}/8086-SSE/f128M_isSignalingNaN.c
		${berkeley}/8086-SSE/s_f64UIToCommonNaN.c
		${berkeley}/8086-SSE/s_commonNaNToF128M.c
		${berkeley}/8086-SSE/s_extF80MToCommonNaN.c
		${berkeley}/8086-SSE/extF80M_isSignalingNaN.c
		${berkeley}/8086-SSE/s_f128MToCommonNaN.c
		${berkeley}/8086-SSE/s_commonNaNToF64UI.c
		${berkeley}/8086-SSE/s_commonNaNToExtF80M.c
		${berkeley}/8086-SSE/softfloat_raiseFlags.c
		${berkeley}/8086-SSE/s_propagateNaNF128M.c
	)

	target_include_directories (SFloat PRIVATE x86_64 ${berkeley}/8086-SSE)
endif ()
target_include_directories (SFloat PRIVATE ${berkeley}/include)
