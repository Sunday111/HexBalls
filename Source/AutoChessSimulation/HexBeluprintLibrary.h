// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <tuple>
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HexBeluprintLibrary.generated.h"

/**
 * 
 */
UCLASS()
class AUTOCHESSSIMULATION_API UHexBeluprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	static FIntPoint GetNeighbor(FIntPoint P, int direction) {
		constexpr int evenOffsets[]
		{
			1, 0,
			1, -1,
			0, -1,
			-1, -1,
			-1, 0,
			0, 1
		};
		constexpr int oddOffsets[]
		{
			1, 1,
			1, 0,
			0, -1,
			-1, 0,
			-1, 1,
			0, 1
		};
		constexpr const int* offsets[2] {
			evenOffsets, oddOffsets
		};

		if (direction < 0 || direction > 5) {
			return FIntPoint(0, 0);
		}

		const int* O = offsets[P.X & 1];
		O += direction * 2;
		const FIntPoint Delta(O[0], O[1]);
		return P + Delta;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure)
	static FVector HexPointToCoord(FIntPoint P, float Radius) {
		float X = Radius * 1.5f * P.X;
		float Y = Radius * FMath::Sqrt(3.0f) * (P.Y + 0.5f * (P.X & 1));
		return FVector(X, Y, 0.0f);
	}

};
