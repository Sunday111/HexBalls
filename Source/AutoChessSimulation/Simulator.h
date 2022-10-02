// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Containers/BitArray.h"
#include "UObject/NoExportTypes.h"
#include "Simulator.generated.h"

USTRUCT(BlueprintType)
struct FEntityInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 Id;

	UPROPERTY(BlueprintReadOnly)
	int32 MaxHealth;

	UPROPERTY(BlueprintReadOnly)
	int32 Health;

	UPROPERTY(BlueprintReadOnly)
	int32 TicksPerMove; // Ticks required to move to the neighbor

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentCellIndex;

	UPROPERTY(BlueprintReadOnly)
	int32 StartedMoveAtStep;

	UPROPERTY(BlueprintReadOnly)
	int32 NextCellIndex;

	UPROPERTY(BlueprintReadOnly)
	int32 Target;

	UPROPERTY(BlueprintReadOnly)
	int32 TimeStepsPerAttack;

	UPROPERTY(BlueprintReadOnly)
	int32 StartedAttackAtStep;

	UPROPERTY(BlueprintReadOnly)
	int32 DestroyAt;
};


UCLASS(BlueprintType)
class AUTOCHESSSIMULATION_API USimulator : public UObject
{
	GENERATED_BODY()
	
public:
	// At least reuse some allocations...
	struct FDijkstraLookup
	{
		// Returns first element of shortest path
		int32 ShortestPath(const USimulator& S, int32 From, int32 To);

		TArray<int32> Dist;
		TBitArray<> Visited;
		TMap<int32, int32> Prev;
		TArray<int32> Queue;
	};

	static constexpr int32 TimeStep = 100; // milliseconds
	static constexpr int32 StepsPerSecond = 1000 / 100;

	UFUNCTION(BlueprintCallable)
	void Initialize();

	UFUNCTION(BlueprintCallable)
	void NextStep();
	
	UFUNCTION(BlueprintCallable)
	float TimeAtStep(int32 Step) const;

	UPROPERTY(BlueprintReadWrite, meta=(ExposeOnSpawn="true"))
	FIntPoint MapSize;

	UPROPERTY(BlueprintReadWrite, meta=(ExposeOnSpawn="true"))
	int32 EntitiesCount;

	UPROPERTY(BlueprintReadWrite, meta=(ExposeOnSpawn="true"))
	int32 Seed;

	UPROPERTY(BlueprintReadOnly)
	int32 StepIndex = 0;

	UPROPERTY(BlueprintReadOnly)
	TArray<FEntityInfo> Entities;

	// Map from entity id to index in Entities array
	TMap<int32, int32> IdToIndex;

	UFUNCTION(BlueprintCallable)
	bool GetEntityInfo(int32 Id, FEntityInfo& EntityInfo);

	UFUNCTION(BlueprintCallable)
	FIntPoint ToPoint(int32 CellIndex) const;
	int32 ToIndex(FIntPoint P) const;

	bool RandomFreeNeighbor(FIntPoint O, int32& CellIndex);
	bool IsValid(FIntPoint) const;
	bool IsOccupied(FIntPoint) const;
	bool IsOccupied(int32) const;

private:
	int32 NextEntityId = 0;
	FRandomStream RandomStream;
	TBitArray<> Occupied;
	float StartTime;
	FDijkstraLookup Dijkstra;
};
