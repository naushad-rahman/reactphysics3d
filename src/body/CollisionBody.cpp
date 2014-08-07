/********************************************************************************
* ReactPhysics3D physics library, http://code.google.com/p/reactphysics3d/      *
* Copyright (c) 2010-2013 Daniel Chappuis                                       *
*********************************************************************************
*                                                                               *
* This software is provided 'as-is', without any express or implied warranty.   *
* In no event will the authors be held liable for any damages arising from the  *
* use of this software.                                                         *
*                                                                               *
* Permission is granted to anyone to use this software for any purpose,         *
* including commercial applications, and to alter it and redistribute it        *
* freely, subject to the following restrictions:                                *
*                                                                               *
* 1. The origin of this software must not be misrepresented; you must not claim *
*    that you wrote the original software. If you use this software in a        *
*    product, an acknowledgment in the product documentation would be           *
*    appreciated but is not required.                                           *
*                                                                               *
* 2. Altered source versions must be plainly marked as such, and must not be    *
*    misrepresented as being the original software.                             *
*                                                                               *
* 3. This notice may not be removed or altered from any source distribution.    *
*                                                                               *
********************************************************************************/

 // Libraries
#include "CollisionBody.h"
#include "engine/CollisionWorld.h"
#include "engine/ContactManifold.h"

// We want to use the ReactPhysics3D namespace
using namespace reactphysics3d;

// Constructor
CollisionBody::CollisionBody(const Transform& transform, CollisionWorld& world, bodyindex id)
              : Body(id), mType(DYNAMIC), mTransform(transform), mProxyCollisionShapes(NULL),
                mNbCollisionShapes(0), mContactManifoldsList(NULL), mWorld(world) {

    mIsCollisionEnabled = true;
    mInterpolationFactor = 0.0;

    // Initialize the old transform
    mOldTransform = transform;
}

// Destructor
CollisionBody::~CollisionBody() {
    assert(mContactManifoldsList == NULL);

    // Remove all the proxy collision shapes of the body
    removeAllCollisionShapes();
}

// Add a collision shape to the body.
/// This methods will create a copy of the collision shape you provided inside the world and
/// return a pointer to the actual collision shape in the world. You can use this pointer to
/// remove the collision from the body. Note that when the body is destroyed, all the collision
/// shapes will also be destroyed automatically. Because an internal copy of the collision shape
/// you provided is performed, you can delete it right after calling this method. The second
/// parameter is the transformation that transform the local-space of the collision shape into
/// the local-space of the body. By default, the second parameter is the identity transform.
/// This method will return a pointer to the proxy collision shape that links the body with
/// the collision shape you have added.
ProxyShape* CollisionBody::addCollisionShape(const CollisionShape& collisionShape,
                                             const Transform& transform) {

    // Create an internal copy of the collision shape into the world (if it does not exist yet)
    CollisionShape* newCollisionShape = mWorld.createCollisionShape(collisionShape);

    // Create a new proxy collision shape to attach the collision shape to the body
    ProxyShape* proxyShape = new (mWorld.mMemoryAllocator.allocate(
                                      sizeof(ProxyShape))) ProxyShape(this, newCollisionShape,
                                                                      transform, decimal(1));

    // Add it to the list of proxy collision shapes of the body
    if (mProxyCollisionShapes == NULL) {
        mProxyCollisionShapes = proxyShape;
    }
    else {
        proxyShape->mNext = mProxyCollisionShapes;
        mProxyCollisionShapes = proxyShape;
    }

    // Compute the world-space AABB of the new collision shape
    AABB aabb;
    newCollisionShape->computeAABB(aabb, mTransform * transform);

    // Notify the collision detection about this new collision shape
    mWorld.mCollisionDetection.addProxyCollisionShape(proxyShape, aabb);

    mNbCollisionShapes++;

    // Return a pointer to the collision shape
    return proxyShape;
}

// Remove a collision shape from the body
void CollisionBody::removeCollisionShape(const ProxyShape* proxyShape) {

    ProxyShape* current = mProxyCollisionShapes;

    // If the the first proxy shape is the one to remove
    if (current == proxyShape) {
        mProxyCollisionShapes = current->mNext;
        mWorld.mCollisionDetection.removeProxyCollisionShape(current);
        mWorld.removeCollisionShape(proxyShape->mCollisionShape);
        current->ProxyShape::~ProxyShape();
        mWorld.mMemoryAllocator.release(current, sizeof(ProxyShape));
        mNbCollisionShapes--;
        return;
    }

    // Look for the proxy shape that contains the collision shape in parameter
    while(current->mNext != NULL) {

        // If we have found the collision shape to remove
        if (current->mNext == proxyShape) {

            // Remove the proxy collision shape
            ProxyShape* elementToRemove = current->mNext;
            current->mNext = elementToRemove->mNext;
            mWorld.mCollisionDetection.removeProxyCollisionShape(elementToRemove);
            mWorld.removeCollisionShape(proxyShape->mCollisionShape);
            elementToRemove->ProxyShape::~ProxyShape();
            mWorld.mMemoryAllocator.release(elementToRemove, sizeof(ProxyShape));
            mNbCollisionShapes--;
            return;
        }

        // Get the next element in the list
        current = current->mNext;
    }

    assert(mNbCollisionShapes >= 0);
}

// Remove all the collision shapes
void CollisionBody::removeAllCollisionShapes() {

    ProxyShape* current = mProxyCollisionShapes;

    // Look for the proxy shape that contains the collision shape in parameter
    while(current != NULL) {

        // Remove the proxy collision shape
        ProxyShape* nextElement = current->mNext;
        mWorld.mCollisionDetection.removeProxyCollisionShape(current);
        mWorld.removeCollisionShape(current->mCollisionShape);
        current->ProxyShape::~ProxyShape();
        mWorld.mMemoryAllocator.release(current, sizeof(ProxyShape));

        // Get the next element in the list
        current = nextElement;
    }

    mProxyCollisionShapes = NULL;
}

// Reset the contact manifold lists
void CollisionBody::resetContactManifoldsList() {

    // Delete the linked list of contact manifolds of that body
    ContactManifoldListElement* currentElement = mContactManifoldsList;
    while (currentElement != NULL) {
        ContactManifoldListElement* nextElement = currentElement->next;

        // Delete the current element
        currentElement->ContactManifoldListElement::~ContactManifoldListElement();
        mWorld.mMemoryAllocator.release(currentElement, sizeof(ContactManifoldListElement));

        currentElement = nextElement;
    }
    mContactManifoldsList = NULL;
}

// Update the broad-phase state for this body (because it has moved for instance)
void CollisionBody::updateBroadPhaseState() const {

    // For all the proxy collision shapes of the body
    for (ProxyShape* shape = mProxyCollisionShapes; shape != NULL; shape = shape->mNext) {

        // Recompute the world-space AABB of the collision shape
        AABB aabb;
        shape->getCollisionShape()->computeAABB(aabb, mTransform *shape->getLocalToBodyTransform());

        // Update the broad-phase state for the proxy collision shape
        mWorld.mCollisionDetection.updateProxyCollisionShape(shape, aabb);
    }
}

// Ask the broad-phase to test again the collision shapes of the body for collision
// (as if the body has moved).
void CollisionBody::askForBroadPhaseCollisionCheck() const {

    // For all the proxy collision shapes of the body
    for (ProxyShape* shape = mProxyCollisionShapes; shape != NULL; shape = shape->mNext) {

        mWorld.mCollisionDetection.askForBroadPhaseCollisionCheck(shape);
    }
}

// Return true if a point is inside the collision body
bool CollisionBody::testPointInside(const Vector3& worldPoint) const {

    // For each collision shape of the body
    for(ProxyShape* shape = mProxyCollisionShapes; shape != NULL; shape = shape->mNext) {

        // Test if the point is inside the collision shape
        if (shape->testPointInside(worldPoint)) return true;
    }

    return false;
}

// Raycast method
bool CollisionBody::raycast(const Ray& ray, decimal distance) {
    // TODO : Implement this method
    return false;
}

// Raycast method with feedback information
bool CollisionBody::raycast(const Ray& ray, RaycastInfo& raycastInfo, decimal distance) {
    // TODO : Implement this method
    return false;
}
