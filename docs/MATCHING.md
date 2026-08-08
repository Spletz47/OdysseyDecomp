# Common Non-Matching Patterns

Quick reference for patterns that cause assembly mismatches.

---

## Optimizations
| Pattern | Symptom |
|---|---|


## Constructors

**Two adjacent `f32` fields not coalescing into one `str x8`**
Ensure both fields are assigned in source (even if zero) and try reordering them to match the target's store order.

## `sead` Types

### Initializers

sead's math types are initialized in many different ways, which can matter
for matching.

Your decompiler may produce code like:
```cpp
*(this + 0x2F) = 0x3F80000000000000LL;
*(this + 0x2E) = 0;
```
Without context from the assembly, this may be either:
```cpp
sead::Quatf _2e = {0.0f, 0.0f, 0.0f, 1.0f}; // or sead::Quatf(0.0f, 0.0f, 0.0f, 1.0f);
sead::Quatf _2e = sead::Quatf::unit;
```
If the assembly does an `ADRL`/`ADRP` load, it's loading the latter.

### Setting vector types

TODO: Figure out the actual patterns here

**operator= vs .set()**

```cpp
// pseudocode
*(_DWORD *)(v1 + 0x1A0) = *(_DWORD *)(v2 + 0x38);
*(_QWORD *)(v1 + 0x198) = *(_QWORD *)(v2 + 0x30);
// cpp
_198 = v2->_30;
_198.set(v2->_30);
```


**Distance calculations**

Symptom: `ldr s2` (z component) appears after the first two `fsub`s instead of before, and/or callee-saved float regs (`d8`/`d9`/`d10`) are missing from the frame.

```asm
; Target
bl      getPlayerPos
ldp     s8, s9, [x0]      ; load all 3 player pos components into callee-saved regs
ldr     s10, [x0, #0x8]
bl      getTrans
ldp     s0, s1, [x0]      ; load all 3 trans components
ldr     s2, [x0, #0x8]    ; <-- z loaded before any fsub
fsub    s1, s9, s1
fsub    s0, s8, s0
fsub    s2, s10, s2
```

```cpp
// Wrong: temporary changes register pressure
if ((a - b).length() < threshold) ...
// Wrong: reference to getTrans delays load of z component
sead::Vector3f playerPos = getPlayerPos(x);
sead::Vector3f diff = playerPos - getTrans(x);
// Right: copy both into plain locals so all components load before subtracting
sead::Vector3f playerPos = getPlayerPos(x);
sead::Vector3f trans = getTrans(x);
sead::Vector3f diff = playerPos - trans;
if (diff.length() < threshold) ...
```

**Cross product: use `setCross`**
```cpp
// Wrong: individual component writes produce wrong schedule
vel.x = a.y*b.z - a.z*b.y; vel.y = ...; vel.z = ...;
// Right
vel.setCross(a, b);
```

**Vector differences: use `operator+` and `operator-`**
```cpp
dir.x = a.x - b.x;
dir.y = a.y - b.y;
dir.z = a.z - b.z;
// can almost always be replaced with
sead::Vector3f dir = a - b;
```

**Accumulating differences: use `operator+=` and `operator-`**
```cpp
// Wrong: individual ldr/fadd/str
dir.x += a.x - b.x; ...   
dir += getTrans(a) - getTrans(b);  // Right: ldp pattern
```

**Negation: avoid unary `operator-`**
```cpp
sead::Vector3f neg = -vel;                      // may materialise temp differently
sead::Vector3f neg = {-vel.x, -vel.y, -vel.z};  // explicit struct literal
```

**`operator*` on a named variable breaks match; passing as argument is fine**
```cpp
sead::Vector3f scaled = diff * scale;   // wrong if stored to named variable
shot(trans, diff * scale);              // fine when passed directly
```

**`sead::Matrix34f` translation column — use `setTranslation()`**
```cpp
mtx.m[0][3] = v.x; mtx.m[1][3] = v.y; mtx.m[2][3] = v.z;  // wrong
mtx.setTranslation(v);                                    // right — ldp+str pattern
```

### Loops

```cpp
auto* group = new al::DeriveActorGroup<Foo>("name", count);
mGroup = group;
for (s32 i = 0; i < group->getMaxActorCount(); i++) { ... }
// Using mGroup inside the loop reloads from memory; the local keeps the register live
```

---

### Constants & Literals

| Ghidra shows | Write as |
|---|---|
| `3.4028235e+38` (FLT_MAX) | `sead::Mathf::maxNumber()` |
| `6.2832f` | `sead::Mathf::pi2()` |
| radian value (e.g. `0.40143f`) | `sead::Mathf::deg2rad(23.0f)` |
| `-0.083333f` | `-1.0f / 12.0f` |
| hex like `0xA0`, `0x73` in nerve/interval calls | decimal (`160`, `115`) |

---

### Control Flow & Register Pressure

**Load member pointer after bulk stores, not before**
```cpp
// Wrong: pointer stays live across bulk stores, forces extra callee-saved reg
MyActor* actor = mActor;
mMtx.setTranslation(pos);
actor->doThing();

// Right
mMtx.setTranslation(pos);
mActor->doThing();
```

**Force evaluation order with local variables**
```cpp
// When original hoists a load before a computation:
MyClass* ptr = mMember;           // load first
const sead::Vector3f& t = al::getTrans(this);  // then getTrans
sead::Vector3f offset = mRadius * dir;         // multiply after
ptr->shot(t, offset);
```

**Unsigned comparison to eliminate a branch**
```cpp
// When assembly shows b.lo / b.hs after a modulo:
if ((u32)level < 3) return sTable[level];
return sTable[2];  // fallthrough; don't return a raw literal
```

### Local Vector Stack Slot Order

When two local `sead::Vector3f` variables have swapped stack slots, swap their declaration order. The compiler assigns stack slots in declaration order.

```cpp
// If original has shotDir at sp+0x10, frontDir at sp+0x20:
sead::Vector3f frontDir;  // declared first to lower address
sead::Vector3f shotDir;
```

### Miscellaneous

**dword/qword index arithmetic**:
`*((_DWORD*)this + N)` = byte offset `N×4`
`*((_QWORD*)this + N)` = byte offset `N×8`
