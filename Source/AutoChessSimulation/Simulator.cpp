#include "Simulator.h"

struct FHexCubeCoords {
    int32 Q;
    int32 R;
    int32 S;

    FHexCubeCoords operator*(int32 V) const {
        return FHexCubeCoords{ Q * V, R * V, S * V };
    }
};

static FHexCubeCoords OddQToCube(FIntPoint P) {
    FHexCubeCoords C;
    C.Q = P.X;
    C.R = P.Y - (P.X - (P.X & 1)) / 2;
    C.S = -C.Q - C.R;
    return C;
}

static FIntPoint CubeToOddQ(FHexCubeCoords C) {
    int32 X = C.Q;
    int32 Y = C.R + (C.Q - (C.Q & 1)) / 2;
    return FIntPoint(X, Y);
}

static int32 ToIndex(FIntPoint P, const FIntPoint& MapSize) {
    return P.X + (P.Y * MapSize.X);
}

static FIntPoint ToPoint(int32 Index, const FIntPoint& MapSize) {
    int32 X = Index % MapSize.X;
    int32 Y = Index / MapSize.X;
    return FIntPoint(X, Y);
}

inline constexpr int8 EvenNeighborsOffsets[]
{
    // except (1, 1), (-1, 1)
    1, 0,
    1, -1,
    0, -1,
    -1, -1,
    -1, 0,
    0, 1
};

inline constexpr int8 OddNeigborsOffsets[]
{
    // except (-1, -1), (1, -1)
    1, 1,
    1, 0,
    0, -1,
    -1, 0,
    -1, 1,
    0, 1
};
inline constexpr const int8* NeighborsOffsets[2] {
    EvenNeighborsOffsets, OddNeigborsOffsets
};

static FIntPoint HexNeighborAt(FIntPoint P, int32 Index) {

    if (Index < 0 || Index > 5) {
        return FIntPoint(0, 0);
    }

    const int8* O = NeighborsOffsets[P.X & 1];
    O += Index * 2;
    const FIntPoint Delta(O[0], O[1]);
    return P + Delta;
}

static int32 HexDistance(FIntPoint A, FIntPoint B) {
    auto CA = OddQToCube(A);
    auto CB = OddQToCube(B);
    return FMath::Max3(FMath::Abs(CA.Q - CB.Q), FMath::Abs(CA.R - CB.R), FMath::Abs(CA.S - CB.S));
}

int32 USimulator::FDijkstraLookup::ShortestPath(const USimulator& S, int32 From, int32 To)
{
    const int32 CellsCount = S.MapSize.X * S.MapSize.Y;
    Dist.Reset();
    Prev.Reset();
    Queue.Reset();

    Visited.Init(false, CellsCount);
    Visited[From] = true;
    
    Dist.SetNumUninitialized(CellsCount);
    for (int32 Ci = 0; Ci != CellsCount; ++Ci)
        Dist[Ci] = TNumericLimits<int32>::Max();
    Dist[From] = 0;

    Queue.Add(From);
    while (Queue.Num() > 0) {
        int32 U = Queue.Pop();
        Visited[U] = true;
        if (U == To)
            break;
        
        for (int32 Ni = 0; Ni != 6; ++Ni) {
            auto PV = HexNeighborAt(S.ToPoint(U), Ni);
            const int32 V = S.ToIndex(PV);
            if (S.IsValid(PV) && !Visited[V] && (V == To || !S.IsOccupied(V))) {
                int32 FindIndex = Algo::LowerBound(Queue, V, [&](int32 V1, int32 V2) {
                    return (Dist[V1] > Dist[V2]) || (V1 < V2);
                });

                if (FindIndex < Queue.Num() && Queue[FindIndex] == V)
                    continue;

                int32 Alt = Dist[U] + 1;
                if (Alt < Dist[V]) {
                    Dist[V] = Alt;
                    Prev.FindOrAdd(V) = U;
                }
                Queue.Insert(V, FindIndex);
            }
        }
    }

    if (Visited[To]) {
        int32 Current = To;
        while (Prev[Current] != From)
            Current = Prev[Current];

        return Current;
    }

    return INDEX_NONE;
}

void USimulator::Initialize() {
    // Initialize random stream with specified seed
    RandomStream = FRandomStream(Seed);

    NextEntityId = 0;
    
    MapSize.X = FMath::Max(MapSize.X, 1);
    MapSize.Y = FMath::Max(MapSize.Y, 1);
    const int32 CellsCount = MapSize.X * MapSize.Y;

    Occupied.Init(false, CellsCount);

    auto randomFreeCellIndex = [&]() {
        int32 CellIndex = RandomStream.RandRange(0, CellsCount - 1);
        while (IsOccupied(CellIndex))
            CellIndex = (CellIndex + 1) % CellsCount;
        return CellIndex;
    };

    // Spawn entities
    EntitiesCount = FMath::Max(0, EntitiesCount);
    for (int32 i = 0; i != EntitiesCount; ++i) {
        int32 EntityId = NextEntityId++;
        int32 CellIndex = randomFreeCellIndex();
        int32 EntityIndex = Entities.Emplace();
        FEntityInfo& EntityInfo = Entities[EntityIndex];
        IdToIndex.Add(EntityId) = EntityIndex;
        
        EntityInfo.Id = EntityId;
        EntityInfo.CurrentCellIndex = CellIndex;
        EntityInfo.MaxHealth = RandomStream.RandRange(2, 5);
        EntityInfo.Health = EntityInfo.MaxHealth;
        EntityInfo.TicksPerMove = RandomStream.RandRange(7, 20);
        EntityInfo.StartedMoveAtStep = 0;
        EntityInfo.NextCellIndex = EntityInfo.CurrentCellIndex;
        EntityInfo.TimeStepsPerAttack = RandomStream.RandRange(20, 30);
        EntityInfo.Target = INDEX_NONE;
        EntityInfo.DestroyAt = INDEX_NONE;
        EntityInfo.StartedAttackAtStep = INDEX_NONE;

        Occupied[EntityInfo.CurrentCellIndex] = true;
    }

    StepIndex = 1;
    StartTime = GetWorld()->GetTimeSeconds();
}

void USimulator::NextStep() {
    constexpr int32 AttackDistance = 1;
    constexpr int32 DestroyDelay = 10;

    const int32 CellsCount = MapSize.X * MapSize.Y;
    bool DeletePass = false;
    for (int32 i = 0; i < Entities.Num(); ++i) {
        FEntityInfo& Entity = Entities[i];
        if (Entity.Health <= 0) {
            if (Entity.DestroyAt == INDEX_NONE)
                Entity.DestroyAt = StepIndex + DestroyDelay;
            else if (Entity.DestroyAt >= StepIndex)
                DeletePass = true;

            continue;
        }

        // If entity has target from previous iterations
        if (Entity.Target != INDEX_NONE) {
            // Ensure that target entity still exists
            if (IdToIndex.Contains(Entity.Target)) {
                FEntityInfo& TargetEntity = Entities[IdToIndex[Entity.Target]];
                int32 Dist = HexDistance(ToPoint(Entity.CurrentCellIndex), ToPoint(TargetEntity.CurrentCellIndex));
                // if current target is alive and in attack range
                if (TargetEntity.Health > 0 && Dist <= AttackDistance) {
                    if (StepIndex - Entity.StartedAttackAtStep >= Entity.TimeStepsPerAttack) {
                        TargetEntity.Health--;
                        Entity.StartedAttackAtStep = StepIndex;
                    }
                    continue;
                }
            }

            // Clear target entity if it does not exist or already dead
            Entity.Target = INDEX_NONE;
        }

        int32 MovingDuration = StepIndex - Entity.StartedMoveAtStep;
        if (Entity.CurrentCellIndex == Entity.NextCellIndex || MovingDuration >= Entity.TicksPerMove) {
            const bool MovedJustNow = Entity.CurrentCellIndex != Entity.NextCellIndex;
            if (MovedJustNow) {
                Occupied[Entity.CurrentCellIndex] = false;
                Occupied[Entity.NextCellIndex] = true;
            }

            // Move an entity
            Entity.CurrentCellIndex = Entity.NextCellIndex;
            Entity.StartedMoveAtStep = StepIndex;

            // Don't look for a target after move -
            // maybe there will be less targets at the next step :)
            if (MovedJustNow)
                continue;

            // Find closest target
            // TODO: Make some clustering to shrink lookup area
            int32 TargetEntityId = INDEX_NONE;
            int32 MinDist = TNumericLimits<int32>::Max();
            for (int32 j = 0; j < Entities.Num(); ++j) {
                if (i != j) {
                    FEntityInfo& AnotherEntity = Entities[j];
                    if (AnotherEntity.Health > 0) {
                        int32 Dist = HexDistance(ToPoint(Entity.CurrentCellIndex), ToPoint(AnotherEntity.CurrentCellIndex));
                        if (Dist < MinDist) {
                            TargetEntityId = AnotherEntity.Id;
                            MinDist = Dist;
                            if (Dist == 1) {
                                break;
                            }
                        }
                    }
                }
            }

            if (TargetEntityId != INDEX_NONE) {
                FEntityInfo& TargetEntity = Entities[IdToIndex[TargetEntityId]];
                if (HexDistance(ToPoint(Entity.CurrentCellIndex), ToPoint(TargetEntity.CurrentCellIndex)) <= AttackDistance) {
                    Entity.NextCellIndex = Entity.CurrentCellIndex;
                    Entity.Target = TargetEntityId;
                    Entity.StartedAttackAtStep = StepIndex;
                } else {
                    Entity.StartedAttackAtStep = INDEX_NONE;
                    Entity.Target = INDEX_NONE;
                    Entity.NextCellIndex = Dijkstra.ShortestPath(*this, Entity.CurrentCellIndex, TargetEntity.CurrentCellIndex);
                    if (Entity.NextCellIndex == INDEX_NONE)
                        Entity.NextCellIndex = Entity.CurrentCellIndex;
                    else {
                        Occupied[Entity.NextCellIndex] = true;
                    }
                }
            }
        }
    }

    if (DeletePass) {
        IdToIndex.Reset();
        for (int32 i = 0; i < Entities.Num(); ++i) {
            FEntityInfo& Entity = Entities[i];
            if (Entity.DestroyAt >= StepIndex) {
                IdToIndex.Remove(Entity.Id);
                Occupied[Entity.CurrentCellIndex] = false;
                Occupied[Entity.NextCellIndex] = false;
                Entities.RemoveAtSwap(i--);
            }
            else {
                IdToIndex.Add(Entity.Id, i);
            }
        }
    }

    ++StepIndex;
}

float USimulator::TimeAtStep(int32 Step) const
{
    return StartTime + Step * (float(TimeStep) / 1000.f);
}

bool USimulator::GetEntityInfo(int32 Id, FEntityInfo& EntityInfo) {
    int32* pIndex = IdToIndex.Find(Id);
    if (pIndex) {
        if (*pIndex < Entities.Num()) {
            EntityInfo = Entities[*pIndex];
            return true;
        }
    }
    return false;
}

FIntPoint USimulator::ToPoint(int32 CellIndex) const {
    return ::ToPoint(CellIndex, MapSize);
}

int32 USimulator::ToIndex(FIntPoint P) const {
    return ::ToIndex(P, MapSize);
}

bool USimulator::RandomFreeNeighbor(FIntPoint O, int32& CellIndex) {
    // Select random start index
    int32 Ni = RandomStream.RandHelper(6);
    for (int32 I = 0; I != 6; ++I) {
        FIntPoint N = HexNeighborAt(O, Ni);
        if (IsValid(N) && !IsOccupied(N)) {
            CellIndex = ToIndex(N);
            return true;
        }
    }

    return false;
}

bool USimulator::IsValid(FIntPoint P) const {
    return
        P.X >= 0 && P.Y >= 0 &&
        P.X < MapSize.X && P.Y < MapSize.Y;
}

bool USimulator::IsOccupied(FIntPoint P) const {
    return IsOccupied(ToIndex(P));
}

bool USimulator::IsOccupied(int32 CellIndex) const {
    return Occupied[CellIndex];
}
