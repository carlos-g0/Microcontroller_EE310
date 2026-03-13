//--------------------------------------------------------
// Title:      EE310 Assignment 4 - First Assembly Program
//--------------------------------------------------------
// Purpose:
//   1) Compare measuredTemp and refTemp:
//        measuredTemp > refTemp  -> contReg=2 -> RD2 ON (Cooling)
//        measuredTemp < refTemp  -> contReg=1 -> RD1 ON (Heating)
//        measuredTemp = refTemp  -> contReg=0 -> RD1/RD2 OFF
//
//   2) Convert refTemp and measuredTemp to decimal digits using
//      repeated subtraction (100s then 10s) and store:
//        refTemp      -> 0x60 ones, 0x61 tens, 0x62 hundreds
//        measuredTemp -> 0x70 ones, 0x71 tens, 0x72 hundreds
//      For measuredTemp, ignore negative sign (use absolute value).
// Dependencies: MyConfigFile, xc.inc
// Compiler: MBLAB X IDE v6.30, pic-as (v3.10)
// Author: Carlos Gonzalez
// OUTPUTS: PORTD.1 (Heating), PORTD.2 (Cooling)
// INPUTS: TEST_REFTEMP and TEST_MEASTEMP constants in code
// Versions:
//	V1.0: 3/5/2026 - Comparing Values
//	V2.0: 3/7/2026 - Decimal Conversion
//	V3.0: 3/11/2026 - Final
// Device: PIC18F47K42
//--------------------------------------------------------

#include "MyConfigFile.inc"
#include <xc.inc>

;----------------------------
; EASY TEST VALUES (change these for demo)
;----------------------------
; refTemp range: +10..+50
TEST_REFTEMP    EQU 15

; measuredTemp range: -10..+60
; If negative, use 2's complement
TEST_MEASTEMP   EQU 0xFB     

;----------------------------
; REQUIRED VARIABLE LOCATIONS
;----------------------------
refTemp       EQU 0x20
measuredTemp  EQU 0x21
contReg       EQU 0x22

;----------------------------
; REQUIRED DECIMAL OUTPUT LOCATIONS
;----------------------------
refOnes       EQU 0x60
refTens       EQU 0x61
refHund       EQU 0x62

measOnes      EQU 0x70
measTens      EQU 0x71
measHund      EQU 0x72

;----------------------------
; Scratch registers (simple)
;----------------------------
diffReg       EQU 0x23
tempVal       EQU 0x24
hundDigit     EQU 0x25
tenDigit      EQU 0x26

;----------------------------
; Reset Vector
;----------------------------
PSECT resetVec,class=CODE,reloc=2
ORG 0x0000
    GOTO main

;----------------------------
; Main Program (must start at 0x20)
;----------------------------
PSECT code,class=CODE,reloc=2
ORG 0x0020

main:
    ; ---- Load test/demo values into required registers ----
    MOVLW   TEST_REFTEMP
    MOVWF   refTemp, 0

    MOVLW   TEST_MEASTEMP
    MOVWF   measuredTemp, 0

    ; ---- Setup PORTD as digital outputs ----
    BANKSEL ANSELD
    CLRF    ANSELD, 0

    BANKSEL TRISD
    CLRF    TRISD, 0

    BANKSEL LATD
    CLRF    LATD, 0

    ; Set BSR to 0
    CLRF    BSR, 0

;--------------------------------------------------------
; Compare (signed): measuredTemp vs refTemp
; Compute: W = measuredTemp - refTemp
; Use flags:
;   Z=1 => equal
;   (N XOR OV)=1 => negative => measured < ref
;--------------------------------------------------------
compareTemps:
    MOVF    refTemp, 0, 0
    SUBWF   measuredTemp, 0, 0     ; WREG = measuredTemp - refTemp
    MOVWF   diffReg, 0

    ; Equal?
    BTFSC   STATUS, 2              ; Z
    GOTO    equalCase

    ; Signed less than test: (N XOR OV)
    BTFSC   STATUS, 4              ; N = 1?
    GOTO    n_is_1
n_is_0:
    ; N=0: if OV=1 => negative => less
    BTFSC   STATUS, 3              ; OV
    GOTO    lessCase
    GOTO    greaterCase
n_is_1:
    ; N=1: if OV=0 => negative => less
    BTFSS   STATUS, 3              ; OV
    GOTO    lessCase
    GOTO    greaterCase

greaterCase:
    ; measured > ref -> cooling
    MOVLW   0x02
    MOVWF   contReg, 0
    BANKSEL LATD
    BCF     LATD, 1
    BSF     LATD, 2
    GOTO    doDecimalWork

lessCase:
    ; measured < ref -> heating
    MOVLW   0x01
    MOVWF   contReg, 0
    BANKSEL LATD
    BSF     LATD, 1
    BCF     LATD, 2
    GOTO    doDecimalWork

equalCase:
    ; measured = ref -> both off
    MOVLW   0x00
    MOVWF   contReg, 0
    BANKSEL LATD
    BCF     LATD, 1
    BCF     LATD, 2

;----------------------------
; Convert temperatures to decimal digits 
;----------------------------
doDecimalWork:

    ; ----- Convert refTemp -----
    MOVF    refTemp, 0, 0
    MOVWF   tempVal, 0
    CALL    convertToDigits
    MOVFF   hundDigit, refHund
    MOVFF   tenDigit,  refTens
    MOVFF   tempVal,   refOnes

    ; ----- Convert measuredTemp -----
    MOVF    measuredTemp, 0, 0
    MOVWF   tempVal, 0

    ; if tempVal is negative (bit7=1), take absolute value
    BTFSS   tempVal, 7              ; test MSB
    GOTO    measPositive
    NEGF    tempVal, 0
measPositive:

    CALL    convertToDigits
    MOVFF   hundDigit, measHund
    MOVFF   tenDigit,  measTens
    MOVFF   tempVal,   measOnes

endHere:
    SLEEP
    
;--------------------------------------------------------
; convertToDigits
; Input:  tempVal (0..255) magnitude
; Output: hundDigit = hundreds digit
;         tenDigit  = tens digit
;         tempVal   = ones digit
; Method: repeated subtraction by 100, then 10
;--------------------------------------------------------
convertToDigits:
    CLRF    hundDigit, 0
    CLRF    tenDigit, 0

hundLoop:
    MOVLW   100
    SUBWF   tempVal, 0, 0           ; WREG = tempVal - 100
    BTFSS   STATUS, 0               ; C=1 => tempVal >= 100
    GOTO    tensLoop
    MOVLW   100
    SUBWF   tempVal, 1, 0           ; tempVal = tempVal - 100
    INCF    hundDigit, 1, 0
    GOTO    hundLoop

tensLoop:
    MOVLW   10
    SUBWF   tempVal, 0, 0           ; WREG = tempVal - 10
    BTFSS   STATUS, 0               ; C=1 => tempVal >= 10
    RETURN
    MOVLW   10
    SUBWF   tempVal, 1, 0           ; tempVal = tempVal - 10
    INCF    tenDigit, 1, 0
    GOTO    tensLoop
    
    END
    
