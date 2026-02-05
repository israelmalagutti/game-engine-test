# 2D Skeletal / Bone Animation

This document is a deep dive into skeletal (bone-based) animation for 2D games. It covers the foundational concepts, the math, the tooling ecosystem, the aesthetic trade-offs, and the implementation architecture. Everything is explained from scratch, with connections to web development concepts where they exist.

You already understand transformation matrices from your sprite rendering system, where each sprite has a model matrix built from `glm::translate` and `glm::scale`. Skeletal animation extends that same idea: instead of one transform per sprite, you build a **hierarchy of transforms** where parent transforms cascade down to children, and each body part sprite gets its final position from the chain of transforms above it in the hierarchy.

## Table of Contents
- [1. What Skeletal Animation Is](#1-what-skeletal-animation-is)
  - [The Core Idea](#the-core-idea)
  - [Forward Kinematics](#forward-kinematics)
  - [The DOM Tree Analogy](#the-dom-tree-analogy)
  - [Why "Skeletal"?](#why-skeletal)
- [2. Bone Hierarchies and Transform Chains](#2-bone-hierarchies-and-transform-chains)
  - [What a Bone Actually Is](#what-a-bone-actually-is)
  - [Local Space vs. World Space](#local-space-vs-world-space)
  - [The Transform Chain](#the-transform-chain)
  - [Complete Character Walkthrough](#complete-character-walkthrough)
  - [Cascading Transforms](#cascading-transforms)
- [3. Skinning: Attaching Art to Bones](#3-skinning-attaching-art-to-bones)
  - [Simple (Rigid) Skinning](#simple-rigid-skinning)
  - [Mesh Skinning (Weighted Vertices)](#mesh-skinning-weighted-vertices)
  - [Why Mesh Skinning Matters](#why-mesh-skinning-matters)
- [4. Keyframe Animation and Interpolation](#4-keyframe-animation-and-interpolation)
  - [What a Keyframe Is](#what-a-keyframe-is)
  - [Linear Interpolation (Lerp)](#linear-interpolation-lerp)
  - [Spherical Linear Interpolation (Slerp)](#spherical-linear-interpolation-slerp)
  - [Bezier Curves for Custom Easing](#bezier-curves-for-custom-easing)
  - [Animation Curves](#animation-curves)
  - [Skeletal vs. Frame-by-Frame](#skeletal-vs-frame-by-frame)
- [5. Animation Blending and Transitions](#5-animation-blending-and-transitions)
  - [Cross-Fading](#cross-fading)
  - [Additive Blending](#additive-blending)
  - [Animation Layers](#animation-layers)
  - [Blend Trees](#blend-trees)
  - [The CSS Transition Analogy](#the-css-transition-analogy)
- [6. Industry Standard Tools](#6-industry-standard-tools)
  - [Spine](#spine)
  - [DragonBones](#dragonbones)
  - [Other Tools](#other-tools)
- [7. The Pixel Art Problem](#7-the-pixel-art-problem)
  - [Rotation Artifacts](#rotation-artifacts)
  - [Sub-Pixel Positioning](#sub-pixel-positioning)
  - [The Puppet Look](#the-puppet-look)
  - [Filtering Dilemma](#filtering-dilemma)
  - [Resolution Dependency](#resolution-dependency)
  - [Partial Solutions](#partial-solutions)
- [8. When Skeletal Animation Works Well](#8-when-skeletal-animation-works-well)
- [9. When Skeletal Animation Does Not Work](#9-when-skeletal-animation-does-not-work)
- [10. Case Studies](#10-case-studies)
  - [Hollow Knight](#hollow-knight)
  - [Darkest Dungeon](#darkest-dungeon)
  - [Dead Cells (The Hybrid Approach)](#dead-cells-the-hybrid-approach)
- [11. Implementation Concepts](#11-implementation-concepts)
  - [Bone Class](#bone-class)
  - [Skeleton Class](#skeleton-class)
  - [AnimationClip](#animationclip)
  - [AnimationPlayer](#animationplayer)
  - [Renderer Integration](#renderer-integration)
  - [Complete Pseudocode](#complete-pseudocode)
- [Glossary](#glossary)

---

## 1. What Skeletal Animation Is

### The Core Idea

In frame-by-frame animation (covered in `docs/sprites/DOCS.md`), you draw every single frame of every single animation by hand. A walk cycle with 8 frames means 8 complete images of the character. This gives you total artistic control, but it is labor-intensive and every animation exists as a fixed, predetermined sequence of images.

Skeletal animation takes a fundamentally different approach. Instead of drawing complete frames, you:

1. **Separate** the character into individual body parts (head, torso, left arm, right arm, left leg, right leg, etc.)
2. **Connect** those body parts with a hierarchy of invisible "bones"
3. **Animate** by changing the position, rotation, and scale of the bones over time
4. **Reconstruct** the complete character image each frame by compositing the body parts according to their bone transforms

```
Frame-by-Frame Approach:

  Frame 1        Frame 2        Frame 3        Frame 4
  (complete)     (complete)     (complete)     (complete)
  ┌──────┐       ┌──────┐       ┌──────┐       ┌──────┐
  │  O   │       │  O   │       │  O   │       │  O   │
  │ /|\  │       │ /|   │       │  |\  │       │  |   │
  │ / \  │       │ / |  │       │  | \ │       │ / \  │
  └──────┘       └──────┘       └──────┘       └──────┘
  Artist draws   Artist draws   Artist draws   Artist draws
  EVERYTHING     EVERYTHING     EVERYTHING     EVERYTHING

Skeletal Approach:

  One set of parts:            Bones change each frame:
  ┌─────┐ ┌─────┐             Frame 1:  Bones at angles A
  │ Head│ │Torso│             Frame 2:  Bones at angles B
  └─────┘ └─────┘             Frame 3:  Bones at angles C
  ┌─────┐ ┌─────┐             Frame 4:  Bones at angles D
  │L Arm│ │R Arm│
  └─────┘ └─────┘             The ENGINE reassembles the parts
  ┌─────┐ ┌─────┐             at the right positions/rotations
  │L Leg│ │R Leg│             every frame.
  └─────┘ └─────┘
```

This is a shift from "pre-rendered" to "runtime-assembled." The artist draws each body part once (or a few variants), and the animator manipulates bones to create unlimited poses from those same parts.

### Forward Kinematics

The standard way skeletal animation works is called **forward kinematics (FK)**. "Kinematics" means the study of motion. "Forward" means you work from the root of the hierarchy outward toward the extremities:

```
Forward Kinematics (FK):

  You rotate the shoulder → the upper arm moves
    → the elbow follows → the forearm moves
      → the wrist follows → the hand moves
        → the fingers follow

Direction of control:  Root → Leaf
Direction of effect:   Parent → Children → Grandchildren
```

You set the rotation of each joint directly, and the positions of everything downstream are computed as a consequence. If you rotate the shoulder by 30 degrees, the elbow, wrist, and fingers all move to new positions, because they are all children (or descendants) of the shoulder bone.

This is the opposite of **inverse kinematics (IK)**, which you may have read about in `docs/sprites/DOCS.md`. In IK, you specify where you want the hand to be, and the system calculates what the shoulder and elbow angles should be to reach that target. FK and IK are complementary -- many animation systems use both.

### The DOM Tree Analogy

If you have worked with web development, you already understand the core concept of skeletal animation. You just did not know it.

Consider this HTML and CSS:

```html
<div class="character" style="transform: translate(200px, 300px);">
  <div class="torso" style="transform: rotate(5deg);">
    <div class="head" style="transform: translate(0, -20px) rotate(-3deg);">
    </div>
    <div class="left-arm" style="transform: translate(-15px, -5px) rotate(45deg);">
      <div class="left-forearm" style="transform: translate(0, 20px) rotate(30deg);">
      </div>
    </div>
  </div>
</div>
```

When you apply `transform: rotate(5deg)` to `.torso`, the `.head` and `.left-arm` and `.left-forearm` all rotate with it, because CSS transforms on a parent element affect all children. If you then rotate `.left-arm` by 45 degrees, that rotation is **relative to** the already-rotated torso. The forearm, being a child of the arm, receives both the torso's rotation and the arm's rotation.

This is **exactly** how skeletal animation works. The bone hierarchy is a tree of transforms, just like the DOM is a tree of elements with transforms. Each bone's transform is local (relative to its parent), and the final world-space transform is the product of all ancestor transforms multiplied together.

```
DOM Tree:                          Bone Hierarchy:

document                          Root (hips)
  └── .character (translate)        ├── Spine
        └── .torso (rotate)         │     ├── Head
              ├── .head (rotate)    │     ├── L_Arm
              └── .left-arm (rot)   │     │     └── L_Forearm
                    └── .forearm    │     └── R_Arm
                                    │           └── R_Forearm
Same concept.                       ├── L_Leg
Same math.                          │     └── L_Shin
Different domain.                   └── R_Leg
                                          └── R_Shin
```

The browser's rendering engine walks the DOM tree from root to leaf, accumulating transforms along the way, to figure out where each element ends up on screen. A game engine's skeletal animation system walks the bone tree from root to leaf, accumulating transforms along the way, to figure out where each body part ends up on screen. The algorithm is identical.

### Why "Skeletal"?

The name comes from an analogy with biological anatomy:

- **Bones** define the structure and control the motion, just like your real skeleton does. A bone in the animation system is an invisible transform node with a parent-child relationship.
- **Skin** is the visible surface that is draped over the bones. In the animation system, the "skin" is the character art (sprites or mesh vertices) that is attached to the bones and moves with them.
- **Joints** are the points where bones connect and rotation happens. In the animation system, a bone's pivot point (the origin of its local coordinate system) acts as the joint.

The skeleton is invisible in the final render. You never see the bones themselves -- you only see the art (the "skin") that is attached to them. The bones are the hidden mechanism that controls the visible surface, just like your real skeleton is invisible but controls your body's shape and movement.

```
What the animator sees             What the player sees
(debug/editor view):               (final render):

       O  ← head bone                   ┌───┐
       |                                 │ O │   ← head sprite
    ───┼───  ← shoulder bones           ┌┤   ├┐
       |                                ││   ││  ← torso sprite
       |  ← spine bone                 ││   ││
      / \                              │└───┘│
     /   \ ← leg bones                │ │ │  │  ← arm sprites
    /     \                            └─┘ └─┘  ← leg sprites
```

---

## 2. Bone Hierarchies and Transform Chains

### What a Bone Actually Is

A bone is not a visual element. It is a **data structure** that stores:

1. **A local transform**: position offset, rotation angle, and scale, all relative to the bone's parent
2. **A parent reference**: which bone is this bone attached to (null for the root)
3. **A computed world transform**: the final combined transform after accounting for all ancestors

In code terms, a bone is roughly:

```cpp
struct Bone {
    // Identity
    std::string name;         // "left_forearm", "head", etc.
    Bone* parent;             // Pointer to parent bone (nullptr for root)

    // Local transform (relative to parent)
    glm::vec2 localPosition;  // Offset from parent's origin
    float localRotation;      // Rotation in degrees, relative to parent
    glm::vec2 localScale;     // Scale relative to parent (usually 1.0, 1.0)

    // Computed result (set by the skeleton update)
    glm::mat4 worldTransform; // Final matrix combining all ancestor transforms
};
```

Note that this is conceptually similar to your existing Sprite's model matrix in `Sprite.cpp`:

```cpp
// Your current sprite rendering builds one model matrix:
glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position.x, position.y, 0.0f));
model = glm::scale(model, glm::vec3(size.x, size.y, 1.0f));
```

A bone does the same thing, but its transform is **relative to its parent**, not relative to the world origin. And the final world transform is the product of the entire chain of ancestor transforms.

### Local Space vs. World Space

This distinction is critical and comes up constantly in game development.

**World space** is the global coordinate system of your game. When you say "the character is at position (200, 300)," that is a world-space coordinate. Your camera, your tilemap, and all your entities share this coordinate system.

**Local space** is a coordinate system relative to a parent. When you say "the head is 20 pixels above the torso," that is a local-space statement. The head's local position is (0, -20) relative to the torso. The head does not know or care where the torso is in world space.

```
World Space:                      Local Space of the Torso:

    (0,0)                            (0,0) ← torso's origin
    ┌──────────────────────┐          │
    │                      │     (-20)├── Head is at local (0, -20)
    │       (200, 250)     │          │
    │          O  head     │     (-5) ├── L_Arm is at local (-15, -5)
    │       (200, 270)     │          │
    │       ───┼───        │      (0) ● ← origin
    │       (200, 300)     │          │
    │          |  torso    │     (+30)├── L_Leg is at local (-10, +30)
    │         / \          │
    │        /   \  legs   │
    │                      │
    └──────────────────────┘

The head's WORLD position is (200, 250).
The head's LOCAL position is (0, -20).
They describe the same point, in different coordinate systems.
```

Why use local space at all? Because it makes animation infinitely easier. If you stored every bone's position in world space, moving the entire character would require updating every single bone. With local space, you move the root bone and everything follows automatically, because every child is defined relative to its parent.

This is identical to the CSS analogy. When you set `transform: translate(200px, 300px)` on the parent `.character` div, all children move with it. You do not need to update each child's position individually. The children are defined in the parent's local coordinate system.

### The Transform Chain

To convert a bone's local transform into a world-space transform, you multiply all the transforms in the chain from the root down to that bone:

```
worldTransform = root.local × spine.local × shoulder.local × upperArm.local × forearm.local

Each multiplication "stacks" one more transformation on top.
Reading right to left: the forearm's local transform is applied first,
then the upper arm's, then the shoulder's, then the spine's, then the root's.
```

In matrix math notation:

```
W_forearm = M_root * M_spine * M_shoulder * M_upperArm * M_forearm

Where:
  W = world transform (the final result)
  M = local transform matrix (translate × rotate × scale for that bone)
```

Or, more efficiently, since you compute parent world transforms first:

```
W_root      = M_root
W_spine     = W_root      * M_spine
W_shoulder  = W_spine     * M_shoulder
W_upperArm  = W_shoulder  * M_upperArm
W_forearm   = W_upperArm  * M_forearm

Each bone's world transform = its parent's world transform × its own local transform
```

This is a recursive relationship that you resolve by traversing the tree from root to leaves. The root has no parent, so its world transform is just its local transform. Every other bone's world transform is `parent.worldTransform * self.localTransform`.

### Complete Character Walkthrough

Let us walk through a complete example with actual numbers. Consider a character with this bone hierarchy:

```
Character Bone Hierarchy:

  Root (hips)
    ├── Torso
    │     ├── Head
    │     └── L_Arm
    │           └── L_Forearm
    └── (other bones omitted for clarity)
```

Starting values:

```
Bone          Local Position    Local Rotation    Local Scale
──────────    ──────────────    ──────────────    ───────────
Root          (200, 300)        0 degrees         (1, 1)
Torso         (0, -30)         5 degrees          (1, 1)
Head          (0, -20)        -3 degrees          (1, 1)
L_Arm         (-15, -25)      45 degrees          (1, 1)
L_Forearm     (0, 20)         30 degrees          (1, 1)
```

Now let us compute each bone's world transform step by step.

**Step 1: Root**

The root has no parent. Its world transform equals its local transform.

```
Root world transform:
  Position: (200, 300)
  Rotation: 0 degrees
  Scale:    (1, 1)

  As a matrix:
  W_root = translate(200, 300) * rotate(0) * scale(1, 1)
         = translate(200, 300)    (rotation of 0 and scale of 1 are identity)

  The root bone is at world position (200, 300) with no rotation.
```

**Step 2: Torso**

The torso is a child of the root. Its world transform is `W_root * M_torso`.

```
Torso local transform:
  translate(0, -30) then rotate(5 degrees)

W_torso = W_root * M_torso

Since root has 0 rotation and (1,1) scale, this simplifies:
  World position = Root position + Torso local offset
                 = (200, 300) + (0, -30)
                 = (200, 270)

  World rotation = Root rotation + Torso local rotation
                 = 0 + 5
                 = 5 degrees

So the torso is at world position (200, 270), rotated 5 degrees.
```

**Step 3: Head**

The head is a child of the torso. Its world transform is `W_torso * M_head`.

```
Head local transform:
  translate(0, -20) then rotate(-3 degrees)

Now the torso is rotated 5 degrees. This means the head's local offset of
(0, -20) is NOT straight up in world space -- it is 5 degrees tilted.

To apply the torso's rotation to the head's offset:
  Rotated offset = rotate_point((0, -20), 5 degrees)

  Using rotation formula:
    x' = x * cos(5) - y * sin(5) = 0 * 0.9962 - (-20) * 0.0872 = 1.744
    y' = x * sin(5) + y * cos(5) = 0 * 0.0872 + (-20) * 0.9962 = -19.924

  Head world position = Torso world position + Rotated offset
                      = (200, 270) + (1.744, -19.924)
                      = (201.744, 250.076)

  Head world rotation = Torso world rotation + Head local rotation
                      = 5 + (-3)
                      = 2 degrees

So the head is at approximately world position (201.7, 250.1), rotated 2 degrees.

Notice how the torso's 5-degree tilt caused the head to drift slightly
to the right (from x=200 to x=201.7). This is the cascading effect
of parent transforms on children.
```

**Step 4: L_Arm (Left Arm)**

The left arm is a child of the torso. Its world transform is `W_torso * M_leftArm`.

```
L_Arm local transform:
  translate(-15, -25) then rotate(45 degrees)

Again, the torso's 5-degree rotation affects the arm's local offset:
  Rotated offset = rotate_point((-15, -25), 5 degrees)

    x' = -15 * cos(5) - (-25) * sin(5) = -15 * 0.9962 + 25 * 0.0872 = -12.763
    y' = -15 * sin(5) + (-25) * cos(5) = -15 * 0.0872 + (-25) * 0.9962 = -26.213

  L_Arm world position = Torso world position + Rotated offset
                       = (200, 270) + (-12.763, -26.213)
                       = (187.237, 243.787)

  L_Arm world rotation = Torso world rotation + L_Arm local rotation
                       = 5 + 45
                       = 50 degrees

The left arm is at approximately (187.2, 243.8), rotated 50 degrees from horizontal.
```

**Step 5: L_Forearm (Left Forearm)**

The forearm is a child of the left arm. Its world transform is `W_leftArm * M_forearm`.

```
L_Forearm local transform:
  translate(0, 20) then rotate(30 degrees)

The left arm is rotated 50 degrees in world space. The forearm's local offset
of (0, 20) gets rotated by those 50 degrees:

  Rotated offset = rotate_point((0, 20), 50 degrees)

    x' = 0 * cos(50) - 20 * sin(50) = 0 - 20 * 0.766 = -15.32
    y' = 0 * sin(50) + 20 * cos(50) = 0 + 20 * 0.6428 = 12.856

  L_Forearm world position = L_Arm world position + Rotated offset
                           = (187.237, 243.787) + (-15.32, 12.856)
                           = (171.917, 256.643)

  L_Forearm world rotation = L_Arm world rotation + L_Forearm local rotation
                           = 50 + 30
                           = 80 degrees

The forearm ends up at approximately (171.9, 256.6), rotated 80 degrees.
```

Here is an ASCII diagram of the resulting character pose with all computed positions:

```
Resulting Character Pose (approximate):

  World Y
  240 ─
       │        (201.7, 250.1)
       │           O  Head (2 deg)
  250 ─│          /
       │  (187.2,│243.8)
       │    *────┤  L_Arm (50 deg)
  260 ─│   /     │(200, 270)
       │  /      ┼  Torso (5 deg)
       │ *       │
  270 ─│(171.9,  │
       │ 256.6)  │
       │L_Forearm│
  280 ─│(80 deg) │
       │         │
       │         * (200, 300)
  300 ─│         Root (hips)
       │
       └──────────────────────
      160  170  180  190  200  210   World X

  Key:
    O = Head
    * = Joint/bone origin
    ┼ = Torso center
    Lines show bone connections
```

The important thing to observe is how rotations **accumulate**. The forearm's 30-degree local rotation became 80 degrees in world space because it inherited 50 degrees from its parent chain (5 from torso + 45 from arm = 50 for arm, then 50 + 30 = 80 for forearm).

### Cascading Transforms

The power of the hierarchy becomes clear when you change a single parent bone. Suppose you change the torso's rotation from 5 degrees to -10 degrees. You do not need to recalculate anything manually for the head, arm, or forearm -- you just re-run the transform chain, and everything downstream updates automatically.

```
Before (torso = 5 degrees):          After (torso = -10 degrees):

         O                                    O
        /                                      \
   *───┤                                        ├───*
  /    ┼                                        ┼    \
 *     |                                        |     *
       |                                        |
       *                                        *

The entire upper body pivots around the torso bone.
Head, arm, forearm all move to new world positions.
Only the torso's local rotation changed -- the children's
local transforms stayed exactly the same.
```

This is exactly like changing a CSS transform on a parent div: all children reposition themselves automatically. In fact, the browser's layout engine performs precisely this same matrix multiplication chain when it computes the final screen positions of transformed DOM elements.

---

## 3. Skinning: Attaching Art to Bones

### Simple (Rigid) Skinning

The simplest way to attach art to bones is **rigid skinning** (also called **rigid binding**). Each body part image is attached to exactly one bone. The sprite follows that bone's world transform completely.

```
Rigid Skinning:

  Each sprite is locked to one bone:

  Head sprite    ←──→  Head bone
  Torso sprite   ←──→  Torso bone
  L_Arm sprite   ←──→  L_Arm bone
  L_Forearm spr  ←──→  L_Forearm bone
  R_Arm sprite   ←──→  R_Arm bone
  R_Forearm spr  ←──→  R_Forearm bone
  L_Leg sprite   ←──→  L_Leg bone
  R_Leg sprite   ←──→  R_Leg bone

  When the L_Arm bone rotates 45 degrees,
  the L_Arm sprite rotates 45 degrees.
  No ambiguity, no blending. One bone, one sprite.
```

In your engine, this would mean using the bone's world transform matrix directly as the sprite's model matrix:

```cpp
// For each bone that has art attached:
void renderBonePart(const Bone& bone, const Texture& partTexture, Shader& shader) {
    // The bone's world transform already includes position, rotation, and scale
    // from the entire parent chain. Use it directly as the model matrix.
    shader.setMat4("model", bone.worldTransform);

    // Bind the body part texture
    partTexture.bind();

    // Draw the quad (same as your existing Sprite::draw)
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
```

Rigid skinning is straightforward to implement and understand. But it has a visible problem: **gaps at joints**.

```
The Joint Gap Problem:

  When the forearm rotates relative to the upper arm,
  a gap appears at the elbow:

  No rotation (looks fine):        30-degree rotation (gap!):

  ┌──────────┐                     ┌──────────┐
  │ Upper Arm│                     │ Upper Arm│
  └──────────┘                     └──────────┘
  ┌──────────┐                          ╲ ← GAP (background shows through)
  │ Forearm  │                      ┌────╲─────┐
  └──────────┘                      │ Forearm  │
                                    └──────────┘

  The two rectangular sprites don't perfectly meet
  when one is rotated. The corner of the forearm
  sprite pulls away from the upper arm sprite.
```

For many 2D games, this is acceptable or can be mitigated by clever art (overlapping the sprites slightly, adding a circular joint piece over the elbow, etc.). But for high-quality animation, you need mesh skinning.

### Mesh Skinning (Weighted Vertices)

Mesh skinning solves the joint gap problem by replacing rigid rectangular sprites with deformable **meshes** whose individual vertices can be influenced by multiple bones.

Instead of one sprite per bone, you have a single mesh (a grid of triangles) covering the entire character. Each vertex in the mesh stores a list of bones that influence it, along with a **weight** for each bone indicating how much influence that bone has.

```
Mesh Skinning Concept:

  A mesh covers the arm and forearm continuously:

  Upper Arm           Elbow Region           Forearm
  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
  │   │   │   │   │ / │ / │ \ │ \ │   │   │   │   │
  │   │   │   │   │/  │/  │  \│  \│   │   │   │   │
  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘

  Vertex weights at the elbow region:

  Far from elbow:   arm bone = 100%,  forearm bone = 0%
  Near elbow:       arm bone = 70%,   forearm bone = 30%
  At elbow:         arm bone = 50%,   forearm bone = 50%
  Past elbow:       arm bone = 30%,   forearm bone = 70%
  Far past elbow:   arm bone = 0%,    forearm bone = 100%
```

The math for computing a vertex's final position with mesh skinning:

```
For each vertex:

  finalPosition = (weight_A * transformA(originalPosition))
                + (weight_B * transformB(originalPosition))
                + (weight_C * transformC(originalPosition))
                + ...

  Where:
    weight_A, weight_B, weight_C are the bone weights (must sum to 1.0)
    transformA, transformB, transformC are the world transforms of the influencing bones
    originalPosition is the vertex's position in the original, undeformed mesh (bind pose)
```

A concrete numerical example:

```
Example: A vertex at the elbow, influenced by two bones

  Original vertex position (in bind pose): (100, 200)
  Bone A (upper arm) weight: 0.6    (60% influence)
  Bone B (forearm)   weight: 0.4    (40% influence)

  Bone A world transform moves this vertex to: (105, 198)
  Bone B world transform moves this vertex to: (108, 210)

  Final vertex position:
    = 0.6 * (105, 198) + 0.4 * (108, 210)
    = (63, 118.8) + (43.2, 84)
    = (106.2, 202.8)

  The vertex ends up at a blended position between where
  Bone A and Bone B would each individually place it.
```

This blending is what creates smooth deformation at joints. Vertices near the elbow get "pulled" by both the upper arm bone and the forearm bone, creating a natural bend instead of a sharp gap.

### Why Mesh Skinning Matters

The difference is visually significant, especially at joints that rotate through large angles:

```
Rigid Skinning (gap at elbow):       Mesh Skinning (smooth bend):

  ┌──────────┐                        ┌──────────┐
  │ Upper Arm│                        │ Upper Arm│
  └──────────┘                        │          └──┐
       ╲  ← visible gap               │    smooth   │
    ┌───╲──────┐                       │     bend    │
    │ Forearm  │                       │   ┌────────┘
    └──────────┘                       │   │Forearm
                                       └───┘

The mesh skinning version has a continuous surface.
Vertices in the elbow region are blended between both bones,
creating a smooth curve instead of a sharp seam.
```

In 2D games, mesh skinning is often used with Spine or similar tools, where the character art is mapped onto a deformable mesh. The mesh triangles stretch and compress as the bones move, creating fluid deformation. This is particularly effective for:

- Capes and cloth that drape naturally
- Faces that can deform for expressions (squash/stretch cheeks, blink by deforming eyelids)
- Tentacles, tails, and other organic appendages
- Any area where rigid parts would look mechanical

The trade-off is complexity. Rigid skinning is trivial to implement (one matrix per sprite). Mesh skinning requires a triangle mesh per character part, weight data per vertex, and a vertex shader that performs the weighted transform blending. Tools like Spine handle the mesh creation and weight painting for you and provide runtimes that do the rendering.

---

## 4. Keyframe Animation and Interpolation

### What a Keyframe Is

A keyframe is a specific bone pose defined at a specific point in time. Between keyframes, the engine **interpolates** (calculates intermediate values) to produce smooth motion.

```
Keyframe Animation Timeline:

  Time:    0.0s         0.3s         0.6s         1.0s
           │            │            │            │
  Key 1    ●────────────●────────────●────────────●  Key 4
           │            │            │            │
  L_Arm    0 deg        45 deg       20 deg       0 deg
  rotation

  What the engine computes at t=0.15s (halfway between Key 1 and Key 2):
    interpolated rotation = lerp(0, 45, 0.5) = 22.5 degrees

  The animator only defined 4 poses. The engine generates
  all frames in between, producing smooth continuous motion.
```

Compare this with frame-by-frame animation:

```
Frame-by-frame: 30 fps animation for 1 second = 30 hand-drawn images
Skeletal keyframe: 4 key poses + interpolation = same 1 second of animation

The keyframe approach requires dramatically less manual work.
The trade-off is that the motion between keyframes is mathematically generated,
not hand-crafted. This can feel "floaty" or "mechanical" if the keyframes
and easing curves are not carefully designed.
```

### Linear Interpolation (Lerp)

Lerp is the simplest interpolation method. Given two values and a parameter `t` between 0 and 1, lerp produces a value that is `t` percent of the way between them.

```
lerp(a, b, t) = a + (b - a) * t

  When t = 0.0: result = a           (at the start)
  When t = 0.5: result = (a + b) / 2 (halfway)
  When t = 1.0: result = b           (at the end)

  This works for position: lerp(vec2(0, 0), vec2(100, 50), 0.3) = vec2(30, 15)
  This works for scale:    lerp(1.0, 2.0, 0.5) = 1.5
```

For position and scale, lerp works correctly. The object moves in a straight line from A to B at a constant rate. This is fine for many animations.

```cpp
// Linear interpolation for position and scale
glm::vec2 lerpVec2(glm::vec2 a, glm::vec2 b, float t) {
    return a + (b - a) * t;
}

float lerpFloat(float a, float b, float t) {
    return a + (b - a) * t;
}
```

### Spherical Linear Interpolation (Slerp)

Lerp does NOT work correctly for rotation, and understanding why is important.

**The short-path problem:**

```
Problem: Interpolating rotation angles with lerp

  Keyframe 1: rotation = 350 degrees
  Keyframe 2: rotation = 10 degrees

  Using lerp at t = 0.5:
    lerp(350, 10, 0.5) = 350 + (10 - 350) * 0.5 = 350 + (-340) * 0.5 = 180 degrees

  But 350 degrees and 10 degrees are only 20 degrees apart!
  (350 + 20 = 370 = 10 when wrapped around)

  Lerp takes the LONG way around (340 degrees of travel)
  instead of the SHORT way (20 degrees of travel).

  Visualized on a circle:

             0/360
              │
       350 ───┼─── 10
          ↖   │   ↗     Short path: 20 degrees (what we want)
            ↖ │ ↗
              ●
              │
         ↙    │   ↘     Long path: 340 degrees (what lerp does)
       ↙      │     ↘
     180──────┼──────
              │
```

For 2D rotation (a single angle), you can fix this by normalizing the angle difference:

```cpp
// Correct angle interpolation for 2D
float lerpAngle(float a, float b, float t) {
    // Find the shortest angular distance
    float diff = b - a;

    // Normalize to [-180, 180] range
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;

    return a + diff * t;
}

// Example:
// lerpAngle(350, 10, 0.5):
//   diff = 10 - 350 = -340
//   Normalize: -340 + 360 = 20
//   result = 350 + 20 * 0.5 = 360 = 0 degrees
//   This correctly goes the short way!
```

In 3D (or when using quaternions for rotation representation), the equivalent operation is called **slerp** -- Spherical Linear Interpolation. Slerp interpolates along the surface of a unit sphere (in quaternion space), always taking the shortest path. For 2D skeletal animation, the angle normalization approach above is sufficient and much simpler than quaternions.

### Bezier Curves for Custom Easing

Linear interpolation produces constant-speed motion, which looks robotic. Real motion has **easing** -- it accelerates and decelerates. Bezier curves let animators define custom speed curves.

```
Linear (constant speed):        Ease-in-out (natural motion):

Value                            Value
  │          ╱                     │            ╱───
  │        ╱                       │          ╱
  │      ╱                         │        ╱
  │    ╱                           │      ╱
  │  ╱                             │   ──╱
  │╱                               │ ╱
  └──────────→ Time                └──────────→ Time

  Constant slope = constant speed    S-curve = slow start, fast middle, slow end
```

A cubic Bezier curve is defined by four control points: P0, P1, P2, P3. P0 is the start (always 0,0), P3 is the end (always 1,1). P1 and P2 are the control handles that shape the curve.

```
Cubic Bezier control points:

  Value (0 to 1)
  1.0 ─────────────────── P3
       │               ╱
       │        ● P2 ╱
       │           ╱
       │         ╱
       │       ╱
       │     ╱
       │   ╱  ● P1
       │ ╱
  0.0 P0─────────────────
       0.0               1.0
              Time (0 to 1)

  P0 = (0, 0)    start
  P1 = (0.2, 0)  controls the start curve (slow departure)
  P2 = (0.8, 1)  controls the end curve (slow arrival)
  P3 = (1, 1)    end

  This creates an ease-in-out curve.
```

The Bezier function maps a linear time `t` to a curved output value:

```cpp
// Cubic Bezier evaluation
// P0 = (0,0), P3 = (1,1) are implicit
// p1x, p1y = control point 1
// p2x, p2y = control point 2
float cubicBezier(float t, float p1x, float p1y, float p2x, float p2y) {
    // The full cubic Bezier formula:
    // B(t) = (1-t)^3 * P0 + 3*(1-t)^2 * t * P1 + 3*(1-t) * t^2 * P2 + t^3 * P3

    // For animation curves, we need to:
    // 1. Find the t that produces the desired x (time) value
    // 2. Evaluate y (value) at that t
    // This requires numerical solving (Newton's method or binary search)
    // because the Bezier parameter t does NOT directly correspond to time.

    // Simplified approximation for common easing:
    float oneMinusT = 1.0f - t;
    float y = 3.0f * oneMinusT * oneMinusT * t * p1y
            + 3.0f * oneMinusT * t * t * p2y
            + t * t * t;
    return y;
}
```

Common easing presets as Bezier control points:

```
Ease-in:         P1 = (0.42, 0),  P2 = (1, 1)       Slow start, fast end
Ease-out:        P1 = (0, 0),     P2 = (0.58, 1)     Fast start, slow end
Ease-in-out:     P1 = (0.42, 0),  P2 = (0.58, 1)     Slow start, slow end
Linear:          P1 = (0, 0),     P2 = (1, 1)         Constant speed

These are the same values used by CSS transition-timing-function!
```

If you have ever written `transition: transform 0.3s ease-in-out` in CSS, you used exactly this system. The browser evaluates a cubic Bezier curve to determine the intermediate values of the CSS property over time. Skeletal animation tools like Spine provide the same curve editor for each bone property.

### Animation Curves

A skeletal animation clip consists of many individual **curves**. Each curve controls one property of one bone over time.

```
Animation "walk_cycle" curve breakdown:

  Bone: L_Arm
  ├── curve: position.x    (3 keyframes)
  ├── curve: position.y    (2 keyframes)
  ├── curve: rotation      (6 keyframes)  ← most keyframes here (main motion)
  ├── curve: scale.x       (1 keyframe)   ← constant, no animation
  └── curve: scale.y       (1 keyframe)   ← constant, no animation

  Bone: L_Forearm
  ├── curve: rotation      (4 keyframes)
  └── (other properties not animated)

  Bone: Head
  ├── curve: rotation      (2 keyframes)  ← subtle head bob
  └── curve: position.y    (2 keyframes)  ← slight vertical motion

  ...and so on for every animated bone.
```

Each curve is independent. You can have 20 keyframes for the arm rotation but only 2 for the head rotation. The engine evaluates each curve separately, finds the two keyframes that bracket the current time, and interpolates between them.

```
Evaluating a curve at time t = 0.45s:

  Keyframes:   K1 at t=0.3   K2 at t=0.6
               value=20      value=60

  Normalized t within this segment:
    local_t = (0.45 - 0.3) / (0.6 - 0.3) = 0.15 / 0.3 = 0.5

  If using linear interpolation:
    value = lerp(20, 60, 0.5) = 40

  If using Bezier easing:
    eased_t = cubicBezier(0.5, p1x, p1y, p2x, p2y)   // might return 0.68
    value = lerp(20, 60, 0.68) = 47.2
```

### Skeletal vs. Frame-by-Frame

Here is a direct comparison of what an animation looks like in data:

```
Frame-by-frame "walk" animation:
  Data: 8 complete 64x64 pixel images = 8 * 64 * 64 * 4 bytes = 131,072 bytes
  Each frame: a complete image of the character in that pose
  Artist work: Draw 8 complete character poses

Skeletal keyframe "walk" animation:
  Data: ~20 keyframes, each is a few floats per bone
        20 keyframes * 8 bones * 3 properties * 4 bytes = 1,920 bytes
  Each keyframe: rotation/position/scale values for each bone
  Artist work: Draw 6-8 body part images once, then pose bones 20 times

  Data size ratio: 131,072 / 1,920 = ~68x smaller animation data

  Art creation cost:
    Frame-by-frame: 8 complete drawings
    Skeletal: 6-8 part drawings + 20 bone poses (posing is much faster than drawing)
```

The skeletal approach wins massively on data size and creation speed. But it loses on artistic precision: the frame-by-frame artist can make each frame a unique, handcrafted masterpiece. The skeletal approach always looks like "the same parts, rearranged." This trade-off is the central tension of 2D skeletal animation.

---

## 5. Animation Blending and Transitions

This is where skeletal animation truly shines and where frame-by-frame animation simply cannot compete. Because skeletal poses are defined as numbers (position, rotation, scale per bone), you can **mathematically blend** between any two poses.

### Cross-Fading

Cross-fading is the most common blending operation. When a character transitions from walking to idle, you do not want an instant snap -- you want a smooth 0.2-second transition.

```
Cross-fade from Walk to Idle over 0.2 seconds:

  Time:     0.0s     0.05s    0.1s     0.15s    0.2s
  Blend:    100% W   75% W    50% W    25% W    0% W
            0% I     25% I    50% I    75% I    100% I

  For EACH bone at each moment:
    blendedRotation = lerp(walkRotation, idleRotation, blendFactor)
    blendedPosition = lerp(walkPosition, idlePosition, blendFactor)
    blendedScale    = lerp(walkScale,    idleScale,    blendFactor)

  The character smoothly shifts from the walk pose to the idle pose
  over 0.2 seconds. Every bone interpolates independently.
```

In code:

```cpp
// Cross-fade between two animation poses
struct BonePose {
    glm::vec2 position;
    float rotation;
    glm::vec2 scale;
};

BonePose blendPoses(const BonePose& poseA, const BonePose& poseB, float blendFactor) {
    BonePose result;
    // blendFactor = 0.0 means 100% poseA
    // blendFactor = 1.0 means 100% poseB
    result.position = glm::mix(poseA.position, poseB.position, blendFactor);
    result.rotation = lerpAngle(poseA.rotation, poseB.rotation, blendFactor);
    result.scale    = glm::mix(poseA.scale, poseB.scale, blendFactor);
    return result;
}
```

With frame-by-frame animation, transitioning from walk to idle is an unsolvable problem. You cannot blend between two pixel images in a meaningful way (you could dissolve/crossfade the pixels, but that looks like a ghostly double-exposure, not a smooth pose change). The best you can do is pick a good frame to cut on and hope it is not jarring.

### Additive Blending

Additive blending **layers** an animation on top of a base animation, rather than replacing it. This is used for effects that should apply regardless of what the base animation is.

```
Additive Blending Example: Breathing

  Base animation: Walk cycle
    Torso rotation at some time: 3 degrees

  Additive animation: Breathing oscillation
    Torso rotation offset: sin(time * 2) * 2 degrees = 1.5 degrees (at this moment)

  Final torso rotation: 3 + 1.5 = 4.5 degrees

  The breathing applies ON TOP of whatever the base animation is doing.
  Switch from walk to idle? The breathing still works.
  Switch from idle to attack? The breathing still works.
  You author the breathing animation once, and it enhances everything.
```

Additive animations store **deltas** (differences from a reference pose) rather than absolute values:

```
Standard animation keyframe:     Additive animation keyframe:
  L_Arm rotation = 45 degrees      L_Arm rotation = +3 degrees
  (absolute value)                  (delta from current value)

  Standard: sets the bone to 45 degrees, overriding whatever was there
  Additive: adds 3 degrees to whatever the bone currently is
```

Common uses for additive blending:
- Breathing (subtle torso/chest oscillation)
- Hit reactions (brief flinch added to any animation)
- Lean into turns (tilt torso based on movement direction)
- Facial expressions (layer a "smile" on top of any body animation)

### Animation Layers

Animation layers allow different parts of the body to play different animations simultaneously. The skeleton is partitioned into zones, and each zone can play its own animation.

```
Animation Layers Example:

  Layer 0 (Lower Body): Walk cycle
    Controls: Hips, L_Leg, R_Leg, L_Shin, R_Shin, L_Foot, R_Foot

  Layer 1 (Upper Body): Attack swing
    Controls: Spine, L_Arm, R_Arm, L_Forearm, R_Forearm, Head

  Result: The character walks while swinging a sword.
  The legs follow the walk animation.
  The torso and arms follow the attack animation.

  ┌─────────────────────────────────────────────┐
  │           Layer 1: Attack                    │
  │    Head ← attack anim                       │
  │    Arms ← attack anim                       │
  │    Torso ← attack anim                      │
  ├─────────────────────────────────────────────┤
  │           Layer 0: Walk                      │
  │    Hips ← walk anim                         │
  │    Legs ← walk anim                         │
  └─────────────────────────────────────────────┘
```

Each layer has a **mask** that defines which bones it controls. Bones not in a layer's mask are unaffected by that layer.

Without skeletal animation, achieving "walk while attacking" in frame-by-frame would require drawing a completely separate set of frames for every combination: walk+attack, idle+attack, run+attack, and so on. With layers, you author the walk and attack animations independently, and the system combines them at runtime.

### Blend Trees

Blend trees allow **parametric** blending between multiple animations based on a continuous variable, not just switching between them.

```
Blend Tree: Locomotion based on speed

  Speed parameter: 0.0 ──────────────────────── 1.0
                   │          │          │          │
                   ▼          ▼          ▼          ▼
                  Idle      Walk       Jog        Run
                   │          │          │          │
                   └────┬─────┘          └────┬─────┘
                        │                     │
                  blend by speed        blend by speed
                        │                     │
                        └──────────┬──────────┘
                                   │
                             Final pose

  At speed 0.0: 100% Idle
  At speed 0.3: 40% Walk + 60% Idle  (blended)
  At speed 0.5: 100% Walk
  At speed 0.7: 60% Jog + 40% Walk   (blended)
  At speed 1.0: 100% Run

  As the character accelerates, the animation seamlessly
  morphs from idle → walk → jog → run with no visible transition.
```

This is how modern 3D games achieve smooth locomotion (The Last of Us, Red Dead Redemption 2). In 2D, blend trees are less common because the "puppet look" of skeletal 2D animation makes the blending artifacts more noticeable. But for higher-resolution 2D games, blend trees are a powerful tool.

### The CSS Transition Analogy

All of these blending concepts map directly to CSS:

```
CSS Transitions                      Skeletal Animation Blending
──────────────                       ──────────────────────────────
transition: transform 0.2s           Cross-fade with 0.2s duration
  ease-in-out                          (blending from animation A to B)

@keyframes breathe {                 Additive breathing animation
  0% { transform: scaleY(1.0); }       (layered on top of any base)
  50% { transform: scaleY(1.02); }
}

The browser interpolates CSS             The game engine interpolates bone
property values between states.          transforms between keyframes.
Same concept, same math.                 Just applied to bones instead
                                         of DOM elements.
```

The browser's CSS transition engine is, in essence, a simplified animation blending system. It cross-fades between property states using easing curves, exactly like skeletal animation cross-fades between bone poses. The difference is scope: CSS transitions handle a few properties on a few elements, while skeletal animation handles dozens of properties (position, rotation, scale) on dozens of bones, potentially blending multiple animations simultaneously.

---

## 6. Industry Standard Tools

### Spine

**Spine** (by Esoteric Software) is the industry-standard tool for 2D skeletal animation. If you are making a 2D game with skeletal animation, Spine is what most studios use.

**What it is:** A desktop application (Windows, Mac, Linux) where you import your character art, create a bone hierarchy, weight paint meshes, and animate using a timeline editor. It exports animation data in its own format (`.skel` binary or `.json`), which your game engine loads using the Spine runtime library.

**The workflow:**

```
Spine Workflow:

  1. IMPORT ART
     Artist draws body parts as separate images (PNG)
     ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐
     │ Head │ │Torso │ │L_Arm │ │L_Leg │  ...etc
     └──────┘ └──────┘ └──────┘ └──────┘

  2. CREATE BONES
     In the Spine editor, place bones to form the skeleton
     Link each image to a bone

  3. WEIGHT MESHES (optional)
     Convert images to meshes (add vertices)
     Paint bone influence weights on each vertex
     Allows smooth deformation at joints

  4. ANIMATE
     Use the timeline to create keyframes
     Move/rotate/scale bones at key times
     Spine shows the interpolated motion in real-time
     Adjust easing curves per keyframe

  5. EXPORT
     Export .skel (binary, small, fast to load)
     or .json (human-readable, larger, slower to load)
     Plus a texture atlas of all body parts packed into one image

  6. LOAD IN ENGINE
     Use spine-c or spine-cpp runtime library
     Load the exported data
     The runtime handles all bone math, interpolation, and rendering
```

**Spine features:**
- **IK constraints:** Define inverse kinematics chains so you can control a hand position and the arm follows
- **Path constraints:** Bones follow a curved path (useful for tails, ropes, snakes)
- **Mesh deformation:** Vertices can be animated directly, in addition to bone influence (for facial expressions, cloth ripple)
- **Skins:** Swap art sets on the same skeleton (different outfits, characters sharing a rig)
- **Events:** Trigger game events at specific animation times (play footstep sound at frame 3, spawn particle at frame 7)
- **Blend modes:** Individual parts can use additive, multiply, or screen blending for visual effects

**Cost and licensing:**
- Spine Essential: $69 (one-time) -- basic features, no mesh deformation
- Spine Professional: $329 (one-time) -- all features including meshes, IK, paths
- The Spine **runtime** libraries (what your game uses to load and render Spine data) are open source (BSD license), free to use in any project
- You need a Spine license to use the editor that creates the animation data

**Games that use Spine:**
- Hollow Knight (Cherry Dynamic)
- Darkest Dungeon (Red Hook Studios)
- Slay the Spire (Mega Crit)
- Hades (Supergiant Games)
- Pyre (Supergiant Games)
- Into the Breach (Subset Games)
- Wildermyth (Worldwalker Games)
- Night in the Woods (Infinite Fall)

### DragonBones

**DragonBones** is a free, open-source alternative to Spine. It was originally created by Egret Technology (a Chinese game engine company) and is now community-maintained.

**Feature comparison:**

```
Feature                   Spine Pro       DragonBones
────────────────────      ──────────      ───────────
Bone animation            Yes             Yes
Mesh deformation          Yes             Yes
IK constraints            Yes             Yes
Blend modes               Yes             Yes
Texture atlas packing     Yes             Yes
Animation events          Yes             Yes
Path constraints          Yes             No
Skin system               Yes             Basic
Timeline curve editor     Excellent       Good
Platform support          Win/Mac/Linux   Win/Mac
Runtime languages         C, C++, Java,   TypeScript,
                          C#, Lua, etc.   C++, Lua
Community/support         Large, active   Smaller
Price                     $69-$329        Free
Export format             .skel/.json     .dbbin/.json
```

DragonBones is a solid choice for hobbyist and indie projects where the Spine license cost is a concern. Its feature set covers the majority of 2D skeletal animation needs. The main disadvantage is a smaller community, fewer tutorials, and less active development.

**Export formats:**
- `.dbbin` -- binary format (fast loading, smaller files)
- `.json` -- text format (human-readable, larger)
- Both include skeleton data, animation data, and texture atlas coordinates

### Other Tools

**COA Tools (Blender plugin):**
Cutout Animation Tools is a free Blender addon that lets you import 2D art as planes, rig them with Blender's bone system, animate in Blender's timeline, and export for game engines. This gives you access to Blender's powerful animation tools (graph editor, NLA editor, constraints) for 2D work. The downside is that Blender has a steep learning curve and is primarily a 3D tool, so the 2D workflow can feel awkward.

**Godot's built-in 2D Skeletal Animation:**
The Godot game engine has a built-in skeletal animation system for 2D. You create `Skeleton2D` and `Bone2D` nodes in the scene tree, attach sprites, and animate using Godot's animation player. This is well-integrated but tied to the Godot engine. If you are building your own engine (as you are), you cannot use Godot's animation system directly, but studying its API design is educational.

**Unity 2D Animation Package:**
Unity's official package for 2D skeletal animation. It includes the Skinning Editor (for creating meshes and weight painting inside Unity), bone creation tools, and integration with Unity's Animator state machine. Like Godot's solution, this is tied to the Unity engine.

**Spriter / Spriter 2:**
An older 2D animation tool by BrashMonkey. Spriter 1 was popular in the early 2010s. Spriter 2 has been in development for a long time. Features are comparable to Spine Essential. Less widely used today.

---

## 7. The Pixel Art Problem

This section is critical for your project, since you are building an HD-2D Stardew Valley clone. The interaction between skeletal animation and pixel art is fraught with aesthetic challenges.

### Rotation Artifacts

Pixel art is drawn on a precise grid. Each pixel is deliberately placed. When you rotate a piece of pixel art by a non-90-degree angle, the pixels no longer align to the screen's pixel grid. The GPU must figure out what color each screen pixel should be when the source pixels do not line up neatly.

```
Original 4x4 pixel art arm (no rotation):

  ┌───┬───┬───┬───┐
  │ . │ X │ X │ . │     X = drawn pixel
  ├───┼───┼───┼───┤     . = transparent
  │ X │ X │ X │ X │
  ├───┼───┼───┼───┤     Clean, crisp grid.
  │ X │ X │ X │ X │     Every pixel is exactly where
  ├───┼───┼───┼───┤     the artist placed it.
  │ . │ X │ X │ . │
  └───┴───┴───┴───┘

Same arm rotated 15 degrees:

  ┌───┬───┬───┬───┬───┐
  │ . │ . │ ? │ ? │ . │     ? = what goes here?
  ├───┼───┼───┼───┼───┤
  │ . │ ? │ X │ ? │ . │     The original pixel boundaries
  ├───┼───┼───┼───┼───┤     no longer align with screen pixels.
  │ ? │ X │ X │ X │ ? │     Each screen pixel might cover parts
  ├───┼───┼───┼───┼───┤     of two or three source pixels.
  │ . │ ? │ X │ ? │ . │
  ├───┼───┼───┼───┼───┤
  │ . │ . │ ? │ . │ . │
  └───┴───┴───┴───┴───┘
```

The question marks are the problem. When a screen pixel straddles two source pixels, the GPU must choose: which color wins?

### Sub-Pixel Positioning

Sub-pixel positioning occurs when a transform places a pixel at a fractional screen coordinate. If the arm's bone says "position at (103.7, 250.3)," the arm sprite starts at a fractional position. The 0.7 and 0.3 fractions mean every pixel in the sprite is offset from the screen grid.

```
Integer position (pixel-perfect):     Fractional position (sub-pixel):

Screen pixel grid:                    Screen pixel grid:

┌───┬───┬───┬───┐                    ┌───┬───┬───┬───┐
│   │ A │ A │   │  Sprite pixel A    │   │   │   │   │
├───┼───┼───┼───┤  fills screen      ├───┼─┬─┼─┬─┼───┤
│   │ B │ B │   │  pixels exactly.   │   │A│A│ │ │   │  Sprite is offset
├───┼───┼───┼───┤  No ambiguity.     ├───┼─┼─┼─┼─┼───┤  by (0.7, 0.3).
│   │   │   │   │                    │   │B│B│ │ │   │  Source pixels
└───┴───┴───┴───┘                    ├───┼─┴─┼─┴─┼───┤  straddle screen
                                     │   │   │   │   │  pixels.
                                     └───┴───┴───┴───┘
```

With sub-pixel positioning, the GPU must blend fractional contributions of source pixels. This creates either blurring (with bilinear filtering) or pixel-crawling artifacts (with nearest-neighbor filtering, where pixels appear to "pop" between positions as the rounding changes).

### The Puppet Look

Even without rotation or sub-pixel issues, skeletal animation in 2D has a distinctive "paper puppet" or "paper doll" aesthetic. Because the body parts are drawn as flat images that rotate around pivot points, the character looks like it is made of jointed cardboard pieces rather than a living being.

```
The puppet look vs. hand-drawn animation:

  Skeletal (puppet-like):            Hand-drawn (organic):

  Frame 1:     Frame 2:              Frame 1:     Frame 2:
  ┌──┐         ┌──┐                  ┌──┐         ╭──╮
  │  │         │  │                  │  │         │  │
  └┬─┘         └┬─┘                  └┬─┘         └┬─┘
   │\            │ \                  │\            │╲
   │ \           │  \                 │ ╲           │ ╲
   │  □          │   □                │  ◇          │  ◇
   │             │                    │╱            │╱
   │             │                    ╱│            ╱│

  The skeletal version moves the      The hand-drawn version can change
  same rectangular arm piece.         the arm's shape, perspective,
  It looks flat, mechanical.          line weight, shading, everything.
```

This is inherent to the technique. The body parts ARE flat images being rotated. You can mitigate it (see Partial Solutions below), but you cannot eliminate it. Some games embrace the puppet aesthetic as a deliberate style choice (Darkest Dungeon). Others minimize it through high resolution art and mesh deformation (Hollow Knight).

### Filtering Dilemma

OpenGL offers two texture filtering modes, and neither is ideal for rotated pixel art:

**Nearest-neighbor filtering (`GL_NEAREST`):**
Each screen pixel samples the single closest texel. This preserves hard edges and the "pixel art look," but rotated sprites develop jagged, staircase-like edges. Pixels appear to snap between positions rather than moving smoothly.

**Bilinear filtering (`GL_LINEAR`):**
Each screen pixel blends the four nearest texels weighted by distance. This produces smooth rotation, but it blurs the pixel art. A 16x16 sprite that was crisp and detailed becomes a smudged mess when rotated.

```
Nearest-Neighbor (rotated):          Bilinear (rotated):

  ░ ░ ░ ░ ░                         ░ ░ ░ ░ ░
  ░ ░ █ █ ░                         ░ ▒ ▓ ▓ ▒
  ░ █ █ █ ░                         ▒ ▓ █ ▓ ▒
  ░ █ █ ░ ░                         ▒ ▓ ▓ ▒ ░
  ░ ░ ░ ░ ░                         ░ ▒ ▒ ░ ░

  Jaggy staircase edges.             Blurry, smudged edges.
  Pixels pop between positions.      Loses the crisp pixel art feel.
  Looks "glitchy."                   Looks "vaseline on the lens."
```

This is a no-win situation for low-resolution pixel art. Both options degrade the art quality in different ways.

### Resolution Dependency

The severity of these problems depends heavily on the art resolution:

```
At 16x16 pixels (classic NES/SNES style):
  - A single misplaced pixel is 6% of the character width
  - Rotation artifacts are extremely visible
  - Sub-pixel positioning is immediately noticeable
  - Skeletal animation rarely works well

At 64x64 pixels (modern indie):
  - A misplaced pixel is 1.5% of the character width
  - Artifacts are noticeable but tolerable
  - Some games use skeletal at this scale (with care)

At 256x256 pixels (Hollow Knight scale):
  - A misplaced pixel is 0.4% of the character width
  - Artifacts are nearly invisible
  - Skeletal animation works very well

At 1080p (Hollow Knight rendered resolution):
  - Characters are hundreds of pixels tall
  - Rotation artifacts are invisible to the naked eye
  - This is where skeletal 2D animation truly shines
```

Hollow Knight runs at 1080p with large, detailed character sprites. At that resolution, rotating a body part by a few degrees produces zero visible artifacts. This is why Hollow Knight's skeletal animation looks so good -- the art resolution is high enough that the technique's weaknesses are invisible.

A 16-bit-style game at 320x240 with 16x16 characters has no such luxury. Every pixel matters, and the rotation artifacts cannot be hidden.

### Partial Solutions

If you want to use skeletal animation with pixel art (despite the challenges), here are strategies to minimize the problems:

1. **Limit rotation to small angles.** A 5-degree rotation has far less artifact than a 45-degree rotation. Many games restrict bone rotations to +/-10 degrees, which is enough for subtle motion (breathing, swaying) without severe artifacts.

2. **Use larger sprites.** The bigger your character sprites, the less visible the artifacts. A 64x64 character handles rotation much better than a 16x16 one.

3. **Round positions to integers.** After computing bone world positions, round to the nearest integer to avoid sub-pixel positioning. This eliminates blurring/crawling but makes motion less smooth (you can see individual pixel steps).

4. **Use mesh deformation instead of rotation.** Instead of rotating an arm sprite, deform a mesh that covers the arm. The mesh vertices can be placed at integer coordinates, preserving pixel alignment while still achieving smooth joint bends.

5. **Accept the aesthetic.** Some games lean into the puppet look and make it part of their visual identity. Darkest Dungeon does not try to hide the fact that characters are jointed puppets -- the limited, deliberate motion IS the aesthetic.

6. **Pre-render rotations.** For key angles (0, 15, 30, 45, 60, 75, 90 degrees), have the artist draw the body part at that angle manually. The engine selects the nearest pre-rendered rotation instead of rotating the sprite mathematically. This gives pixel-perfect art at the cost of limited rotation granularity and more art assets.

---

## 8. When Skeletal Animation Works Well

Skeletal animation is the right choice when your game has these characteristics:

**Side-view games (platformers, fighters).**
The character is always seen from one angle. You need only one set of body part images, and the skeleton works in 2D space without needing to handle multiple viewing directions. Hollow Knight, Darkest Dungeon, Dead Cells, and fighting games all benefit from this.

**Higher resolution 2D art.**
When sprites are 128x128 pixels or larger, rotation artifacts become negligible. Games like Hollow Knight, Ori and the Blind Forest, and Rayman Legends all use large, detailed sprites where skeletal animation looks natural.

**Characters that need many animations.**
If a character has 20+ animation states (idle, walk, run, jump, fall, land, attack1, attack2, attack3, dash, climb, swim, hurt, die, pickup, throw, etc.), creating all of these frame-by-frame is enormous work. With a skeleton, you create one rig and animate it in as many states as you want. Each new animation is just a new set of keyframes for the same bones.

**Games with animation blending needs.**
If your game requires smooth transitions between states, layered body animations (legs walk while torso attacks), or parametric blend trees (speed controls the locomotion blend), skeletal animation is the only practical approach. Frame-by-frame animation cannot blend.

**Small teams with limited artists.**
One set of body parts can produce unlimited animations. An animator (who does not need to be a great artist) can create new animations by posing bones, without needing to draw new art. This is a huge productivity multiplier for small teams.

**Characters with customization.**
The skeleton + skin system naturally supports swapping body parts. Different heads, different arms, different torsos can all use the same skeleton. This is similar to the layered composite approach but with the added benefit of animation blending.

---

## 9. When Skeletal Animation Does Not Work

Skeletal animation is the wrong choice when:

**Top-down games.**
In a top-down game, the character is viewed from above and must face four (or eight) directions. Each direction requires a different set of body part images (the "facing down" torso looks completely different from the "facing right" torso). You effectively need a separate rig per direction, which eliminates the main advantage of skeletal animation (one rig for unlimited animations). Frame-by-frame with horizontal mirroring is usually more practical for top-down games. This is why Stardew Valley, Zelda, Pokemon, and most top-down RPGs use frame-by-frame.

```
Top-Down Direction Problem:

  Facing Down:       Facing Right:      Facing Up:
  ┌──────┐           ┌──────┐           ┌──────┐
  │ O    │           │  >   │           │      │
  │/|\   │           │  |── │           │  |   │
  │/ \   │           │  |   │           │ /|\  │
  └──────┘           └──────┘           └──────┘

  These are NOT the same parts rotated.
  They are completely different drawings.
  The "facing down" torso shows the chest.
  The "facing right" torso shows the side.
  The "facing up" torso shows the back.

  You need separate art for each direction,
  which means separate rigs, which means
  you are doing 4x the animation work.
```

**Low-resolution pixel art (16x16 or 32x32).**
At these resolutions, rotation artifacts are impossible to hide. Every pixel is a significant percentage of the character's detail. Rotating even by a few degrees creates visible jaggedness or blur that destroys the hand-placed precision of pixel art.

**Games where every frame must be pixel-perfect.**
Some games have an artistic vision that demands absolute control over every pixel of every frame. The Castlevania series (GBA/DS era), Celeste, and many retro-styled games fall into this category. Skeletal animation cannot guarantee pixel-perfect output because the interpolation and rotation introduce sub-pixel positions and anti-aliased edges.

**Simple games with few animation states.**
If your character has 3 animations (idle, walk, attack) with 4 frames each, that is 12 total frames. Drawing 12 frames is trivial. Setting up a skeletal rig, creating bones, weight-painting meshes, and integrating a runtime library is dramatically more work for such a simple case. The overhead of skeletal animation only pays off when you have many animations.

---

## 10. Case Studies

### Hollow Knight

**Hollow Knight** (Team Cherry, 2017) is one of the most successful examples of 2D skeletal animation in games. It uses Spine for its character animation.

**How it works:**
The game combines skeletal body animation with frame-by-frame effects. The main character (the Knight) and most enemies have their bodies animated with Spine rigs -- the limbs, torso, and head are separate parts connected by bones. But attack effects (sword slashes), environmental particles, and certain stylized animations use traditional frame-by-frame sprites.

```
Hollow Knight Animation Architecture:

  Character Body:         Effects:
  ┌────────────────┐      ┌────────────────┐
  │ Spine Skeleton │      │ Frame-by-Frame │
  │                │      │                │
  │ - Body bones   │      │ - Slash arcs   │
  │ - Limb rotation│      │ - Particles    │
  │ - Squash/stretch│     │ - Impact sparks│
  │ - Blend between│      │ - Spell effects│
  │   walk/idle/etc│      │                │
  └────────────────┘      └────────────────┘
         │                        │
         └───────┬────────────────┘
                 ▼
           Final composite frame
```

**Why the puppet look works for Hollow Knight:**
The characters in Hollow Knight are thin, insect-like creatures with hard exoskeletons. Their bodies naturally look like rigid plates connected at joints. The "paper puppet" aesthetic of skeletal animation matches the physical reality of insect anatomy -- beetles, mantises, and ants ARE essentially rigid plates connected by joints. The art style turns the technique's main weakness into a deliberate strength.

**Squash and stretch:**
Hollow Knight makes heavy use of squash and stretch to add life to its skeletal animation. When the Knight lands from a jump, the body briefly compresses vertically and stretches horizontally. When launching into a dash, the body elongates in the direction of movement. These are quick scale changes on the root or torso bone that last only 2-3 frames.

```
Squash and Stretch on Landing:

  Falling:          Landing (squash):     Recovery (stretch):    Normal:
  ┌──┐              ┌────┐                ┌─┐                   ┌──┐
  │  │              │    │                │ │                   │  │
  │  │              │    │                │ │                   │  │
  │  │              └────┘                │ │                   │  │
  │  │                                    │ │                   │  │
  └──┘              scaleX = 1.3          │ │                   └──┘
                    scaleY = 0.7          └─┘
  scaleX = 1.0                            scaleX = 0.8         scaleX = 1.0
  scaleY = 1.0                            scaleY = 1.2         scaleY = 1.0

  Duration: 1 frame     Duration: 2 frames    Duration: 2 frames

  The total volume (area) stays roughly constant:
    1.0 * 1.0 = 1.0
    1.3 * 0.7 = 0.91
    0.8 * 1.2 = 0.96
  This is the animation principle of "conservation of volume."
```

Squash and stretch is trivial with skeletal animation (just change scale keyframes) but extremely tedious with frame-by-frame (you would need to redraw the entire character at each squash/stretch level).

### Darkest Dungeon

**Darkest Dungeon** (Red Hook Studios, 2016) uses Spine skeletal animation with a very distinctive, deliberately limited style.

**How it uses skeletal animation:**
Every character (heroes and enemies) is a Spine rig with relatively few bones. The animations are intentionally stiff and restricted -- characters do not move fluidly. They shift weight, lift weapons with deliberate slowness, and sway slightly in idle. The limited animation range is a conscious artistic choice.

**How the limited movement creates mood:**
The game's theme is oppressive dread. Characters move like they are exhausted, injured, and terrified. This is achieved not by complex animation, but by restraint:

```
Darkest Dungeon Animation Philosophy:

  Typical game:                    Darkest Dungeon:
  ┌────────────────────┐           ┌────────────────────┐
  │ Character moves    │           │ Character barely    │
  │ fluidly, many bone │           │ moves. Slight sway, │
  │ rotations, snappy  │           │ slow weight shift,  │
  │ responsive motion. │           │ deliberate stiffness│
  │                    │           │                     │
  │ Feels: energetic,  │           │ Feels: exhausted,   │
  │ empowered, fun     │           │ burdened, stressed  │
  └────────────────────┘           └────────────────────┘

  The animation style IS the game feel.
  Skeletal animation's "puppet stiffness" is a feature here.
```

The art style also helps. Characters are drawn in a thick-lined, comic-book style with heavy shadows and angular shapes. This style is forgiving of skeletal artifacts because the art is not trying to be pixel-perfect -- it is going for a rough, hand-inked aesthetic.

**Spine usage in production:**
Red Hook has spoken about their pipeline: artists draw character parts in Photoshop, rig them in Spine, and animators work in Spine's timeline. The Spine runtime renders directly in the game engine. Having Spine as the core animation tool allowed a relatively small team to create a large roster of uniquely animated characters.

### Dead Cells (The Hybrid Approach)

**Dead Cells** (Motion Twin, 2018) uses what is arguably the most sophisticated animation pipeline in 2D gaming. The result is pixel art that moves with a fluidity and weight that pure frame-by-frame or pure skeletal approaches cannot match.

**The full pipeline:**

```
Dead Cells Animation Pipeline:

  Step 1: 3D MODELING
  ┌─────────────────────────┐
  │ Build a 3D model of the │
  │ character in a 3D tool  │
  │ (low-poly, basic)       │
  └────────────┬────────────┘
               ▼
  Step 2: 3D ANIMATION
  ┌─────────────────────────┐
  │ Rig the 3D model with   │
  │ a skeleton. Animate     │
  │ using 3D tools (FK, IK, │
  │ motion curves, etc.)    │
  └────────────┬────────────┘
               ▼
  Step 3: RENDER TO 2D FRAMES
  ┌─────────────────────────┐
  │ Render the 3D animation │
  │ from a side view camera │
  │ at the target pixel     │
  │ resolution (e.g., 48px  │
  │ tall). Output: a sprite │
  │ sheet of raw renders.   │
  └────────────┬────────────┘
               ▼
  Step 4: ARTIST PAINT-OVER
  ┌─────────────────────────┐
  │ A pixel artist manually │
  │ paints over EVERY frame │
  │ of the rendered output. │
  │ Fixes pixel alignment,  │
  │ adds detail, adjusts    │
  │ colors, adds stylistic  │
  │ touches. The 3D render  │
  │ is a reference, not     │
  │ the final output.       │
  └────────────┬────────────┘
               ▼
  Step 5: FINAL PIXEL ART FRAMES
  ┌─────────────────────────┐
  │ The result is a hand-   │
  │ crafted pixel art       │
  │ sprite sheet where each │
  │ frame is pixel-perfect  │
  │ but the motion has the  │
  │ quality of 3D skeletal  │
  │ animation.              │
  └─────────────────────────┘
```

**Why this produces the best pixel art animation:**

The 3D phase gives them all the benefits of skeletal animation:
- Consistent proportions across all frames (the 3D model does not change size)
- Smooth arcs and natural easing (3D animation tools have excellent curve editors)
- Correct perspective and foreshortening (the 3D model handles depth automatically)
- Fast iteration on timing and motion (adjust a curve, re-render, instant result)

The paint-over phase gives them pixel art quality:
- Every pixel is placed deliberately by an artist
- No rotation artifacts (the artist draws at the final pixel resolution)
- Consistent art style across all frames
- Artistic interpretation adds personality that raw renders lack

**The astronomical labor cost:**
This pipeline is incredibly expensive. Every animation frame is touched by BOTH a 3D animator AND a 2D pixel artist. A sword swing that takes 12 frames means:
- Modeling and rigging the 3D character (one-time, but still significant)
- Animating the swing in 3D (hours of work)
- Rendering 12 frames
- Pixel artist paints over all 12 frames (the most time-consuming step)

Dead Cells has several hundred unique animations across its enemies and the player character. Each was hand-painted. This is why the game was in development for years and required a dedicated team of artists.

**When to use this approach:**
Only if your game is a commercial product with a large enough budget to support dedicated 3D animators AND dedicated pixel artists. For a solo developer or small hobby team, this pipeline is not practical. But it is worth understanding as the gold standard of 2D animation quality.

---

## 11. Implementation Concepts

This section describes the architecture you would need to implement skeletal animation in your engine. The code is presented as C++ pseudocode that matches your existing engine's style (using GLM, OpenGL 3.3, your Shader class, etc.).

### Bone Class

A bone stores its identity, its local transform, and its computed world transform.

```cpp
// Bone.h
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

class Bone {
public:
    // Identity
    std::string name;
    int index;                          // Index in the skeleton's bone array
    Bone* parent;                       // Non-owning pointer (skeleton owns all bones)
    std::vector<Bone*> children;        // Non-owning pointers to child bones

    // Local transform (relative to parent)
    glm::vec2 localPosition;            // Offset from parent origin
    float localRotation;                // Rotation in degrees, relative to parent
    glm::vec2 localScale;               // Scale relative to parent

    // Bind pose (the default "rest" pose used as reference)
    glm::vec2 bindPosition;
    float bindRotation;
    glm::vec2 bindScale;

    // Computed world transform (set by Skeleton::updateWorldTransforms)
    glm::mat4 worldTransform;

    // Constructor
    Bone(const std::string& boneName, int boneIndex, Bone* parentBone)
        : name(boneName)
        , index(boneIndex)
        , parent(parentBone)
        , localPosition(0.0f, 0.0f)
        , localRotation(0.0f)
        , localScale(1.0f, 1.0f)
        , bindPosition(0.0f, 0.0f)
        , bindRotation(0.0f)
        , bindScale(1.0f, 1.0f)
        , worldTransform(1.0f)           // Identity matrix
    {
        // If this bone has a parent, register as a child
        if (parent != nullptr) {
            parent->children.push_back(this);
        }
    }

    // Compute this bone's local transform as a 4x4 matrix
    glm::mat4 computeLocalMatrix() const {
        // Start with identity
        glm::mat4 local = glm::mat4(1.0f);

        // Apply translation (offset from parent)
        local = glm::translate(local, glm::vec3(localPosition, 0.0f));

        // Apply rotation around the Z axis (2D rotation)
        local = glm::rotate(local, glm::radians(localRotation), glm::vec3(0.0f, 0.0f, 1.0f));

        // Apply scale
        local = glm::scale(local, glm::vec3(localScale, 1.0f));

        return local;
    }

    // Reset local transform to the bind pose
    void resetToBindPose() {
        localPosition = bindPosition;
        localRotation = bindRotation;
        localScale = bindScale;
    }
};
```

### Skeleton Class

The skeleton owns all bones and provides the `updateWorldTransforms()` method that walks the tree.

```cpp
// Skeleton.h
#pragma once

#include "Bone.h"
#include <vector>
#include <memory>
#include <unordered_map>

class Skeleton {
public:
    // All bones, stored in order (parents always before children)
    std::vector<std::unique_ptr<Bone>> bones;

    // Name-to-bone lookup for convenience
    std::unordered_map<std::string, Bone*> boneMap;

    // The root bone (always bones[0])
    Bone* root;

    // Add a bone to the skeleton
    Bone* addBone(const std::string& name, Bone* parent) {
        int index = static_cast<int>(bones.size());
        auto bone = std::make_unique<Bone>(name, index, parent);
        Bone* bonePtr = bone.get();
        bones.push_back(std::move(bone));
        boneMap[name] = bonePtr;

        // First bone added becomes the root
        if (index == 0) {
            root = bonePtr;
        }

        return bonePtr;
    }

    // Find a bone by name (returns nullptr if not found)
    Bone* findBone(const std::string& name) const {
        auto it = boneMap.find(name);
        if (it != boneMap.end()) {
            return it->second;
        }
        return nullptr;
    }

    // Update all bones' world transforms by walking the hierarchy
    // This MUST be called every frame after animation has set local transforms
    void updateWorldTransforms() {
        // Because bones are stored in order (parents before children),
        // a simple linear traversal guarantees every parent is updated
        // before its children.
        for (auto& bone : bones) {
            if (bone->parent == nullptr) {
                // Root bone: world transform = local transform
                bone->worldTransform = bone->computeLocalMatrix();
            } else {
                // Child bone: world = parent's world * own local
                bone->worldTransform = bone->parent->worldTransform * bone->computeLocalMatrix();
            }
        }
    }

    // Reset all bones to their bind pose
    void resetToBindPose() {
        for (auto& bone : bones) {
            bone->resetToBindPose();
        }
    }
};
```

The key insight in `updateWorldTransforms()` is that bones are stored in a flat array, ordered so that every parent appears before its children. This lets you update the entire hierarchy with a single linear pass, no recursion needed. Each bone just looks up its parent's already-computed world transform and multiplies its own local transform onto it.

This is a common pattern in game engines called **topological ordering** -- you sort the dependency graph so that every node is processed after all its dependencies.

### AnimationClip

An animation clip is a collection of keyframes organized per bone per property.

```cpp
// AnimationClip.h
#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

// A single keyframe: a value at a specific time
struct Keyframe {
    float time;                // Time in seconds
    float value;               // The value at this time (position component, rotation, scale component)

    // Easing curve control points (cubic Bezier)
    // Default to linear (control points on the diagonal)
    glm::vec2 controlOut;      // Outgoing handle (from this keyframe)
    glm::vec2 controlIn;       // Incoming handle (to the next keyframe)

    Keyframe()
        : time(0.0f)
        , value(0.0f)
        , controlOut(0.33f, 0.33f)  // Default: linear
        , controlIn(0.66f, 0.66f)   // Default: linear
    {}

    Keyframe(float t, float v)
        : time(t)
        , value(v)
        , controlOut(0.33f, 0.33f)
        , controlIn(0.66f, 0.66f)
    {}
};

// A curve is a sorted list of keyframes for one property of one bone
struct AnimationCurve {
    std::vector<Keyframe> keyframes;

    // Evaluate the curve at a given time
    float evaluate(float time) const {
        if (keyframes.empty()) return 0.0f;

        // Before the first keyframe: return first value
        if (time <= keyframes.front().time) {
            return keyframes.front().value;
        }

        // After the last keyframe: return last value
        if (time >= keyframes.back().time) {
            return keyframes.back().value;
        }

        // Find the two keyframes that bracket the current time
        for (size_t i = 0; i < keyframes.size() - 1; i++) {
            const Keyframe& k1 = keyframes[i];
            const Keyframe& k2 = keyframes[i + 1];

            if (time >= k1.time && time <= k2.time) {
                // Compute normalized t within this segment (0 to 1)
                float segmentDuration = k2.time - k1.time;
                float localT = (time - k1.time) / segmentDuration;

                // Apply easing curve (cubic Bezier)
                float easedT = evaluateCubicBezier(localT,
                    k1.controlOut.x, k1.controlOut.y,
                    k2.controlIn.x, k2.controlIn.y);

                // Linearly interpolate the value using the eased t
                return k1.value + (k2.value - k1.value) * easedT;
            }
        }

        return keyframes.back().value;
    }

private:
    // Evaluate a cubic Bezier curve for easing
    // P0 = (0,0), P3 = (1,1)
    // This is an approximation; a full implementation would use
    // Newton's method to solve for the x-axis parameter.
    float evaluateCubicBezier(float t, float p1x, float p1y, float p2x, float p2y) const {
        float oneMinusT = 1.0f - t;
        float tt = t * t;
        float ttt = tt * t;
        float oneMinusTT = oneMinusT * oneMinusT;
        float oneMinusTTT = oneMinusTT * oneMinusT;

        // Bezier formula for Y (the output value)
        float y = 3.0f * oneMinusTT * t * p1y
                + 3.0f * oneMinusT * tt * p2y
                + ttt;

        return y;
    }
};

// Tracks for one bone: curves for each animatable property
struct BoneTrack {
    AnimationCurve positionX;
    AnimationCurve positionY;
    AnimationCurve rotation;       // In degrees
    AnimationCurve scaleX;
    AnimationCurve scaleY;
};

// A complete animation clip
class AnimationClip {
public:
    std::string name;              // "walk", "idle", "attack", etc.
    float duration;                // Total duration in seconds
    bool looping;                  // Whether the animation loops

    // Per-bone animation tracks, keyed by bone name
    std::unordered_map<std::string, BoneTrack> boneTracks;

    AnimationClip()
        : name("")
        , duration(0.0f)
        , looping(true)
    {}

    AnimationClip(const std::string& clipName, float clipDuration, bool loop)
        : name(clipName)
        , duration(clipDuration)
        , looping(loop)
    {}

    // Apply this clip's poses to a skeleton at the given time
    void applyToSkeleton(Skeleton& skeleton, float time) const {
        // Wrap time if looping
        float evalTime = time;
        if (looping && duration > 0.0f) {
            evalTime = fmod(time, duration);
            if (evalTime < 0.0f) evalTime += duration;
        } else {
            // Clamp to duration for non-looping clips
            if (evalTime > duration) evalTime = duration;
            if (evalTime < 0.0f) evalTime = 0.0f;
        }

        // For each bone track, evaluate curves and set bone local transforms
        for (const auto& [boneName, track] : boneTracks) {
            Bone* bone = skeleton.findBone(boneName);
            if (bone == nullptr) continue;

            // Evaluate each property curve at the current time
            if (!track.positionX.keyframes.empty()) {
                bone->localPosition.x = track.positionX.evaluate(evalTime);
            }
            if (!track.positionY.keyframes.empty()) {
                bone->localPosition.y = track.positionY.evaluate(evalTime);
            }
            if (!track.rotation.keyframes.empty()) {
                bone->localRotation = track.rotation.evaluate(evalTime);
            }
            if (!track.scaleX.keyframes.empty()) {
                bone->localScale.x = track.scaleX.evaluate(evalTime);
            }
            if (!track.scaleY.keyframes.empty()) {
                bone->localScale.y = track.scaleY.evaluate(evalTime);
            }
        }
    }
};
```

### AnimationPlayer

The animation player manages playback state, current clip, blending, and transitions.

```cpp
// AnimationPlayer.h
#pragma once

#include "AnimationClip.h"
#include "Skeleton.h"
#include <unordered_map>
#include <string>
#include <memory>

class AnimationPlayer {
public:
    // Reference to the skeleton this player controls
    Skeleton& skeleton;

    // All available animation clips
    std::unordered_map<std::string, AnimationClip> clips;

    // Current playback state
    AnimationClip* currentClip;        // The clip currently playing
    float currentTime;                 // Current playback position in seconds
    float playbackSpeed;               // 1.0 = normal, 0.5 = half speed, etc.

    // Blending state for cross-fading
    AnimationClip* previousClip;       // The clip we are blending FROM
    float previousTime;                // Playback position in the previous clip
    float blendDuration;               // How long the cross-fade takes (seconds)
    float blendElapsed;                // How long the cross-fade has been running
    bool isBlending;                   // Whether a cross-fade is in progress

    AnimationPlayer(Skeleton& skel)
        : skeleton(skel)
        , currentClip(nullptr)
        , currentTime(0.0f)
        , playbackSpeed(1.0f)
        , previousClip(nullptr)
        , previousTime(0.0f)
        , blendDuration(0.0f)
        , blendElapsed(0.0f)
        , isBlending(false)
    {}

    // Add an animation clip to the player's library
    void addClip(const AnimationClip& clip) {
        clips[clip.name] = clip;
    }

    // Play a clip immediately (no blending)
    void play(const std::string& clipName) {
        auto it = clips.find(clipName);
        if (it == clips.end()) return;

        currentClip = &it->second;
        currentTime = 0.0f;
        isBlending = false;
    }

    // Transition to a new clip with cross-fading
    void crossFadeTo(const std::string& clipName, float fadeDuration) {
        auto it = clips.find(clipName);
        if (it == clips.end()) return;

        // If already playing this clip, do nothing
        if (currentClip == &it->second && !isBlending) return;

        // Set up the blend
        previousClip = currentClip;
        previousTime = currentTime;
        currentClip = &it->second;
        currentTime = 0.0f;
        blendDuration = fadeDuration;
        blendElapsed = 0.0f;
        isBlending = true;
    }

    // Update the animation state (call every frame with deltaTime)
    void update(float deltaTime) {
        if (currentClip == nullptr) return;

        // Advance playback time
        currentTime += deltaTime * playbackSpeed;

        if (isBlending) {
            // Also advance the previous clip's time (so it keeps moving during blend)
            previousTime += deltaTime * playbackSpeed;
            blendElapsed += deltaTime;

            // Calculate blend factor (0 = all previous, 1 = all current)
            float blendFactor = blendElapsed / blendDuration;
            if (blendFactor >= 1.0f) {
                blendFactor = 1.0f;
                isBlending = false;
            }

            // Apply the previous clip to the skeleton
            previousClip->applyToSkeleton(skeleton, previousTime);

            // Store the previous clip's bone poses
            // (In a real implementation, you'd store these in a temporary buffer)
            std::unordered_map<std::string, BonePose> prevPoses;
            for (const auto& bone : skeleton.bones) {
                BonePose pose;
                pose.position = bone->localPosition;
                pose.rotation = bone->localRotation;
                pose.scale = bone->localScale;
                prevPoses[bone->name] = pose;
            }

            // Apply the current clip to the skeleton
            currentClip->applyToSkeleton(skeleton, currentTime);

            // Blend between previous and current poses
            for (auto& bone : skeleton.bones) {
                auto it2 = prevPoses.find(bone->name);
                if (it2 != prevPoses.end()) {
                    const BonePose& prev = it2->second;
                    // Lerp position
                    bone->localPosition = glm::mix(prev.position, bone->localPosition, blendFactor);
                    // Lerp rotation (using angle interpolation for correctness)
                    bone->localRotation = lerpAngle(prev.rotation, bone->localRotation, blendFactor);
                    // Lerp scale
                    bone->localScale = glm::mix(prev.scale, bone->localScale, blendFactor);
                }
            }
        } else {
            // No blending, just apply the current clip directly
            currentClip->applyToSkeleton(skeleton, currentTime);
        }

        // Update the skeleton's world transforms after all bone locals are set
        skeleton.updateWorldTransforms();
    }

private:
    // Helper struct for storing bone poses during blending
    struct BonePose {
        glm::vec2 position;
        float rotation;
        glm::vec2 scale;
    };

    // Interpolate angles correctly (shortest path)
    float lerpAngle(float a, float b, float t) {
        float diff = b - a;
        while (diff > 180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        return a + diff * t;
    }
};
```

### Renderer Integration

The final step is rendering. For each bone that has art attached, you use the bone's world transform as the sprite's model matrix. This connects the skeletal animation system to your existing rendering pipeline.

```cpp
// SkeletalRenderer.h
#pragma once

#include "Skeleton.h"
#include "Shader.h"
#include "Texture.h"
#include <unordered_map>

// Associates a bone with the texture to draw at that bone's position
struct BoneAttachment {
    std::string boneName;       // Which bone this art is attached to
    Texture* texture;           // The body part image (non-owning, Game owns textures)
    glm::vec2 offset;           // Offset from bone origin to sprite center
    glm::vec2 size;             // Size of the sprite in pixels
    int drawOrder;              // Higher = drawn on top (for layering body parts)
};

class SkeletalRenderer {
public:
    // All body part attachments
    std::vector<BoneAttachment> attachments;

    // OpenGL rendering state (same as your Sprite class)
    GLuint vao;
    GLuint vbo;

    SkeletalRenderer() : vao(0), vbo(0) {
        setupQuad();
    }

    ~SkeletalRenderer() {
        if (vao != 0) glDeleteVertexArrays(1, &vao);
        if (vbo != 0) glDeleteBuffers(1, &vbo);
    }

    // Add a body part attachment
    void attach(const std::string& boneName, Texture* texture,
                glm::vec2 offset, glm::vec2 size, int drawOrder) {
        attachments.push_back({boneName, texture, offset, size, drawOrder});

        // Sort by draw order so back parts are drawn first
        std::sort(attachments.begin(), attachments.end(),
            [](const BoneAttachment& a, const BoneAttachment& b) {
                return a.drawOrder < b.drawOrder;
            });
    }

    // Render all body parts using their bones' world transforms
    void render(const Skeleton& skeleton, Shader& shader, const glm::mat4& projection) {
        shader.use();
        shader.setMat4("projection", projection);

        glBindVertexArray(vao);

        for (const auto& attachment : attachments) {
            // Find the bone this part is attached to
            Bone* bone = skeleton.findBone(attachment.boneName);
            if (bone == nullptr) continue;

            // Build the model matrix from the bone's world transform
            // The bone's world transform already includes the full parent chain.
            // We add the attachment offset and size on top of it.
            glm::mat4 model = bone->worldTransform;

            // Apply the attachment offset (shift the sprite relative to the bone origin)
            model = glm::translate(model, glm::vec3(attachment.offset, 0.0f));

            // Apply the sprite size (scale the unit quad to the body part dimensions)
            model = glm::scale(model, glm::vec3(attachment.size, 1.0f));

            // Upload the model matrix to the shader
            shader.setMat4("model", model);

            // Bind the body part texture
            attachment.texture->bind();

            // Draw the quad
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glBindVertexArray(0);
    }

private:
    // Set up a unit quad (same geometry as your Sprite class)
    void setupQuad() {
        // A quad from (0,0) to (1,1) with UV coordinates
        float vertices[] = {
            // Position (x, y)    UV (u, v)
            0.0f, 0.0f,          0.0f, 0.0f,   // bottom-left
            1.0f, 0.0f,          1.0f, 0.0f,   // bottom-right
            1.0f, 1.0f,          1.0f, 1.0f,   // top-right

            0.0f, 0.0f,          0.0f, 0.0f,   // bottom-left
            1.0f, 1.0f,          1.0f, 1.0f,   // top-right
            0.0f, 1.0f,          0.0f, 1.0f    // top-left
        };

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Position attribute (location 0)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // UV attribute (location 1)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }
};
```

### Complete Pseudocode

Here is how all the pieces fit together in a complete usage example, showing setup and the game loop:

```cpp
// Example: Setting up and running a skeletal character

// ========================================================
// SETUP (done once at initialization)
// ========================================================

// 1. Create the skeleton with a bone hierarchy
Skeleton skeleton;

// Root bone (hips) -- positioned at the character's world location
Bone* root    = skeleton.addBone("root",      nullptr);
Bone* torso   = skeleton.addBone("torso",     root);
Bone* head    = skeleton.addBone("head",      torso);
Bone* lArm    = skeleton.addBone("l_arm",     torso);
Bone* lForearm= skeleton.addBone("l_forearm", lArm);
Bone* rArm    = skeleton.addBone("r_arm",     torso);
Bone* rForearm= skeleton.addBone("r_forearm", rArm);
Bone* lLeg    = skeleton.addBone("l_leg",     root);
Bone* lShin   = skeleton.addBone("l_shin",    lLeg);
Bone* rLeg    = skeleton.addBone("r_leg",     root);
Bone* rShin   = skeleton.addBone("r_shin",    rLeg);

// 2. Set bind pose positions (the default "rest" pose)
root->bindPosition     = glm::vec2(200.0f, 300.0f);
torso->bindPosition    = glm::vec2(0.0f, -30.0f);
head->bindPosition     = glm::vec2(0.0f, -25.0f);
lArm->bindPosition     = glm::vec2(-15.0f, -20.0f);
lForearm->bindPosition = glm::vec2(0.0f, 20.0f);
rArm->bindPosition     = glm::vec2(15.0f, -20.0f);
rForearm->bindPosition = glm::vec2(0.0f, 20.0f);
lLeg->bindPosition     = glm::vec2(-8.0f, 5.0f);
lShin->bindPosition    = glm::vec2(0.0f, 25.0f);
rLeg->bindPosition     = glm::vec2(8.0f, 5.0f);
rShin->bindPosition    = glm::vec2(0.0f, 25.0f);

// Copy bind pose to local transforms
skeleton.resetToBindPose();

// 3. Create animation clips
AnimationClip idleClip("idle", 2.0f, true);    // 2-second loop

// Add a subtle breathing oscillation to the torso
BoneTrack& idleTorsoTrack = idleClip.boneTracks["torso"];
idleTorsoTrack.rotation.keyframes = {
    Keyframe(0.0f,  0.0f),      // Start: no rotation
    Keyframe(1.0f,  2.0f),      // Midpoint: slight lean
    Keyframe(2.0f,  0.0f)       // End: back to start (loops seamlessly)
};
// Subtle vertical bob
idleTorsoTrack.positionY.keyframes = {
    Keyframe(0.0f, -30.0f),     // Start at bind pose Y
    Keyframe(1.0f, -31.5f),     // Slightly higher (remember Y-up means more negative = higher)
    Keyframe(2.0f, -30.0f)      // Back to start
};

AnimationClip walkClip("walk", 0.6f, true);    // 0.6-second loop

// Left leg swings forward and back
BoneTrack& walkLLegTrack = walkClip.boneTracks["l_leg"];
walkLLegTrack.rotation.keyframes = {
    Keyframe(0.0f,   20.0f),    // Forward
    Keyframe(0.3f,  -20.0f),    // Backward
    Keyframe(0.6f,   20.0f)     // Forward again (loop)
};

// Right leg swings opposite to left
BoneTrack& walkRLegTrack = walkClip.boneTracks["r_leg"];
walkRLegTrack.rotation.keyframes = {
    Keyframe(0.0f,  -20.0f),    // Backward (opposite of left)
    Keyframe(0.3f,   20.0f),    // Forward
    Keyframe(0.6f,  -20.0f)     // Backward again
};

// Shins bend at the knee during walk
BoneTrack& walkLShinTrack = walkClip.boneTracks["l_shin"];
walkLShinTrack.rotation.keyframes = {
    Keyframe(0.0f,   0.0f),
    Keyframe(0.15f, 30.0f),     // Knee bends as leg swings back
    Keyframe(0.3f,   0.0f),
    Keyframe(0.45f,  5.0f),
    Keyframe(0.6f,   0.0f)
};

BoneTrack& walkRShinTrack = walkClip.boneTracks["r_shin"];
walkRShinTrack.rotation.keyframes = {
    Keyframe(0.0f,   0.0f),
    Keyframe(0.15f,  5.0f),
    Keyframe(0.3f,   0.0f),
    Keyframe(0.45f, 30.0f),     // Opposite timing from left
    Keyframe(0.6f,   0.0f)
};

// Arms swing opposite to legs (natural walking motion)
BoneTrack& walkLArmTrack = walkClip.boneTracks["l_arm"];
walkLArmTrack.rotation.keyframes = {
    Keyframe(0.0f,  -15.0f),    // Swings opposite to left leg
    Keyframe(0.3f,   15.0f),
    Keyframe(0.6f,  -15.0f)
};

BoneTrack& walkRArmTrack = walkClip.boneTracks["r_arm"];
walkRArmTrack.rotation.keyframes = {
    Keyframe(0.0f,   15.0f),    // Opposite to right leg
    Keyframe(0.3f,  -15.0f),
    Keyframe(0.6f,   15.0f)
};

// Vertical bob during walk (character bounces slightly)
BoneTrack& walkRootTrack = walkClip.boneTracks["root"];
walkRootTrack.positionY.keyframes = {
    Keyframe(0.0f,  300.0f),
    Keyframe(0.15f, 298.0f),    // Slightly up at mid-step
    Keyframe(0.3f,  300.0f),
    Keyframe(0.45f, 298.0f),    // Up again at the other mid-step
    Keyframe(0.6f,  300.0f)
};

// 4. Set up the animation player
AnimationPlayer player(skeleton);
player.addClip(idleClip);
player.addClip(walkClip);
player.play("idle");

// 5. Set up the renderer with body part textures
SkeletalRenderer renderer;
// Assume textures were loaded by Game (your existing system)
renderer.attach("torso",     torsoTexture,    glm::vec2(-12, -15), glm::vec2(24, 30),  1);
renderer.attach("head",      headTexture,     glm::vec2(-10, -20), glm::vec2(20, 20),  2);
renderer.attach("l_arm",     armTexture,      glm::vec2(-5, 0),    glm::vec2(10, 20),  0);
renderer.attach("l_forearm", forearmTexture,   glm::vec2(-4, 0),    glm::vec2(8, 20),   0);
renderer.attach("r_arm",     armTexture,      glm::vec2(-5, 0),    glm::vec2(10, 20),  2);
renderer.attach("r_forearm", forearmTexture,   glm::vec2(-4, 0),    glm::vec2(8, 20),   2);
renderer.attach("l_leg",     legTexture,      glm::vec2(-5, 0),    glm::vec2(10, 25),  1);
renderer.attach("l_shin",    shinTexture,     glm::vec2(-4, 0),    glm::vec2(8, 25),   1);
renderer.attach("r_leg",     legTexture,      glm::vec2(-5, 0),    glm::vec2(10, 25),  1);
renderer.attach("r_shin",    shinTexture,     glm::vec2(-4, 0),    glm::vec2(8, 25),   1);

// ========================================================
// GAME LOOP (every frame)
// ========================================================

// In processInput():
if (movementInput) {
    // Transition to walk with a 0.15-second cross-fade
    player.crossFadeTo("walk", 0.15f);
} else {
    // Transition back to idle
    player.crossFadeTo("idle", 0.2f);
}

// In update(deltaTime):
// Move the character's root bone based on input
Bone* rootBone = skeleton.findBone("root");
rootBone->localPosition.x += velocity.x * deltaTime;

// Update the animation player (advances time, evaluates curves, updates skeleton)
player.update(deltaTime);

// In render():
// The renderer uses each bone's world transform as the model matrix
glm::mat4 projection = glm::ortho(0.0f, 800.0f, 600.0f, 0.0f);
renderer.render(skeleton, spriteShader, projection);
```

The flow is:

```
Every frame:

  1. Process input → decide which animation to play
                      (call player.crossFadeTo() on state changes)

  2. Update animation player → advances time
                                → evaluates keyframe curves
                                → sets each bone's local transforms
                                → blends between clips if cross-fading
                                → calls skeleton.updateWorldTransforms()
                                  (computes world matrix for every bone)

  3. Render → for each body part attachment:
               → get the bone's world transform matrix
               → use it as the model matrix in the shader
               → bind the body part texture
               → draw the quad

  This is the same processInput → update → render loop you already have.
  The skeletal system just adds a layer of bone math between
  "game logic decides state" and "sprite gets drawn at a position."
```

---

## Glossary

| Term | Definition |
|------|-----------|
| **Bone** | A transform node in a hierarchy. Stores local position, rotation, scale, and a parent reference. |
| **Skeleton** | A tree of bones that defines a character's articulation structure. |
| **Bind pose** | The default rest pose of the skeleton, used as the reference for all animations. |
| **Local transform** | A bone's position, rotation, and scale relative to its parent bone. |
| **World transform** | A bone's final position, rotation, and scale in the game world, computed by multiplying all ancestor local transforms. |
| **Forward kinematics (FK)** | Computing child bone positions from parent bone rotations. You control joints, end positions follow. |
| **Inverse kinematics (IK)** | Computing joint rotations from a desired end position. You control the target, joints are calculated. |
| **Rigid skinning** | Each sprite part is attached to exactly one bone. Simple but causes gaps at joints. |
| **Mesh skinning** | Vertices are influenced by multiple bones via weights. Creates smooth deformation at joints. |
| **Weight painting** | The process of assigning bone influence weights to mesh vertices. |
| **Keyframe** | A specific bone pose at a specific time. The engine interpolates between keyframes. |
| **Lerp** | Linear interpolation. `lerp(a, b, t) = a + (b - a) * t`. Used for position and scale. |
| **Slerp** | Spherical linear interpolation. Interpolates rotations along the shortest path on a circle/sphere. |
| **Animation curve** | A function that maps time to a value, defined by keyframes and easing. One curve per bone per property. |
| **Animation clip** | A complete animation (e.g., "walk"), consisting of curves for all animated bones. |
| **Cross-fade** | Blending from one animation to another over a short duration for smooth transitions. |
| **Additive blending** | Layering an animation's deltas on top of a base animation (e.g., breathing on top of walk). |
| **Animation layer** | A partitioning of the skeleton that allows different body regions to play different animations simultaneously. |
| **Blend tree** | A system that parametrically blends between multiple animations based on a continuous variable (e.g., speed). |
| **Topological ordering** | Storing bones so that every parent appears before its children, enabling single-pass world transform computation. |
| **Puppet look** | The characteristic "paper doll" appearance of 2D skeletal animation, where flat body parts rotate around joints. |
| **Sub-pixel positioning** | When a transform places a pixel at a fractional screen coordinate, causing blurring or popping artifacts. |
| **Squash and stretch** | An animation principle where objects compress on impact and elongate during fast motion, applied via scale keyframes on bones. |
| **Spine** | The industry-standard commercial tool for 2D skeletal animation (by Esoteric Software). |
| **DragonBones** | A free, open-source alternative to Spine for 2D skeletal animation. |
| **Draw order** | The z-order in which body parts are rendered. Back parts (like the back arm) are drawn first, front parts last. |
