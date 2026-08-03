# Karaoke Company Security Rules Draft

## Purpose
Draft rules and strategy for Firestore and Storage to enforce company isolation and role-based access.

This model must support two modes at the same time:
- Standalone users who sign in and use a single venue, without any company membership.
- Company users who belong to a multi-venue company tenancy.

## Firestore Rules Draft

```javascript
rules_version = '2';
service cloud.firestore {
  match /databases/{database}/documents {

    function isSignedIn() {
      return request.auth != null;
    }

    function authUid() {
      return request.auth.uid;
    }

    function isPlatformAdmin() {
      return isSignedIn() && request.auth.token.platformAdmin == true;
    }

    function companyRole(companyId) {
      return get(/databases/$(database)/documents/companies/$(companyId)/members/$(authUid())).data.role;
    }

    function isCompanyMember(companyId) {
      return isSignedIn()
        && exists(/databases/$(database)/documents/companies/$(companyId)/members/$(authUid()));
    }

    function isCompanyAdmin(companyId) {
      return isCompanyMember(companyId)
        && companyRole(companyId) in ['company_admin'];
    }

    function isHost(companyId) {
      return isCompanyMember(companyId)
        && companyRole(companyId) in ['host', 'lead_host'];
    }

    function sameCompany(companyId) {
      return request.resource.data.companyId == companyId;
    }

    function legacyVenueLookupPath(venueId) {
      return /databases/$(database)/documents/user-venue-lookup/$(request.auth.uid + "_" + venueId);
    }

    function isLegacyVenueMember(venueId) {
      return isSignedIn()
        && exists(legacyVenueLookupPath(venueId))
        && get(legacyVenueLookupPath(venueId)).data.status == 'active';
    }

    function legacyVenueRole(venueId) {
      return get(legacyVenueLookupPath(venueId)).data.role;
    }

    function isLegacyVenueOperator(venueId) {
      return isLegacyVenueMember(venueId)
        && legacyVenueRole(venueId) in ['Host', 'Admin', 'Tester', 'EnterpriseAdmin'];
    }

    function isLegacyVenueAdmin(venueId) {
      return isLegacyVenueMember(venueId)
        && legacyVenueRole(venueId) in ['Admin', 'Tester', 'EnterpriseAdmin'];
    }

    // Standalone / legacy paths
    match /hosts/{hostId} {
      allow read, create, update: if isPlatformAdmin() || authUid() == hostId;
      allow delete: if isPlatformAdmin();
    }

    match /user-venue-lookup/{lookupId} {
      allow read: if isPlatformAdmin()
        || (isSignedIn() && resource.data.userId == authUid())
        || (isSignedIn() && lookupId == authUid() + "_" + resource.data.venueId);

      allow update: if isPlatformAdmin()
        || (isSignedIn()
          && resource.data.userId == authUid()
          && request.resource.data.diff(resource.data).changedKeys().hasOnly(['lastActive']));

      allow create, delete: if isPlatformAdmin();
    }

    match /venueInvitations/{invitationId} {
      allow read: if isPlatformAdmin()
        || (isSignedIn() && resource.data.invitedUserEmail == request.auth.token.email)
        || (isSignedIn() && resource.data.invitedUserId == authUid())
        || (isSignedIn() && isLegacyVenueAdmin(resource.data.venueId));

      allow create: if isPlatformAdmin()
        || (isSignedIn() && isLegacyVenueAdmin(request.resource.data.venueId));

      allow update, delete: if isPlatformAdmin()
        || (isSignedIn() && isLegacyVenueAdmin(resource.data.venueId));
    }

    match /venueJoinRequests/{requestId} {
      allow read: if isPlatformAdmin()
        || (isSignedIn() && resource.data.requestedByUserId == authUid())
        || (isSignedIn() && isLegacyVenueAdmin(resource.data.venueId));

      allow create: if isSignedIn()
        && request.resource.data.requestedByUserId == authUid();

      allow update, delete: if isPlatformAdmin()
        || (isSignedIn() && resource.data.requestedByUserId == authUid())
        || (isSignedIn() && isLegacyVenueAdmin(resource.data.venueId));
    }

    match /venues/{venueId} {
      allow read: if isPlatformAdmin() || isLegacyVenueMember(venueId);
      allow create, update, delete: if isPlatformAdmin() || isLegacyVenueAdmin(venueId);

      match /{document=**} {
        allow read: if isPlatformAdmin() || isLegacyVenueMember(venueId);
        allow write: if isPlatformAdmin() || isLegacyVenueOperator(venueId);
      }
    }

    match /companies/{companyId} {
      allow read: if isPlatformAdmin() || isCompanyMember(companyId);
      allow create: if isPlatformAdmin();
      allow update, delete: if isPlatformAdmin() || isCompanyAdmin(companyId);

      match /members/{memberId} {
        allow read: if isPlatformAdmin() || isCompanyAdmin(companyId) || authUid() == memberId;
        allow write: if isPlatformAdmin() || isCompanyAdmin(companyId);
      }

      match /venues/{venueId} {
        allow read: if isPlatformAdmin() || isCompanyMember(companyId);
        allow create, update, delete: if isPlatformAdmin() || (isCompanyAdmin(companyId) && sameCompany(companyId));
      }

      match /hosts/{hostId} {
        allow read: if isPlatformAdmin() || isCompanyAdmin(companyId) || authUid() == hostId;
        allow create, update, delete: if isPlatformAdmin() || (isCompanyAdmin(companyId) && sameCompany(companyId));
      }

      match /devices/{deviceId} {
        allow read: if isPlatformAdmin() || isCompanyAdmin(companyId) || isHost(companyId);

        // Host app heartbeat only for own assigned device doc.
        allow update: if isSignedIn()
          && request.resource.data.companyId == companyId
          && request.resource.data.deviceId == deviceId
          && (
            isPlatformAdmin()
            || isCompanyAdmin(companyId)
            || (
              isHost(companyId)
              && request.resource.data.diff(resource.data).changedKeys().hasOnly(['lastSeenAt', 'appVersion', 'metadata'])
            )
          );

        allow create, delete: if isPlatformAdmin() || isCompanyAdmin(companyId);
      }

      match /songPackages/{packageId} {
        allow read: if isPlatformAdmin() || isCompanyMember(companyId);
        // Restrict direct publish mutations to server paths.
        allow create, update, delete: if isPlatformAdmin() || isCompanyAdmin(companyId);
      }

      match /campaigns/{campaignId} {
        allow read: if isPlatformAdmin() || isCompanyMember(companyId);
        allow create, update, delete: if isPlatformAdmin() || isCompanyAdmin(companyId);
      }

      match /deviceUpdateStatus/{statusId} {
        allow read: if isPlatformAdmin() || isCompanyAdmin(companyId) || isHost(companyId);

        // Host app can only update its own device row with bounded fields.
        allow update: if isSignedIn()
          && request.resource.data.companyId == companyId
          && (
            isPlatformAdmin()
            || isCompanyAdmin(companyId)
            || (
              isHost(companyId)
              && request.resource.data.diff(resource.data).changedKeys().hasOnly([
                'state',
                'progressPercent',
                'bytesDownloaded',
                'errorCode',
                'errorMessage',
                'updatedAt',
                'installedAt'
              ])
            )
          );

        allow create, delete: if isPlatformAdmin() || isCompanyAdmin(companyId);
      }

      match /audits/{auditId} {
        allow read: if isPlatformAdmin() || isCompanyAdmin(companyId);
        allow create: if isPlatformAdmin() || isCompanyAdmin(companyId);
        allow update, delete: if isPlatformAdmin();
      }
    }

    // Fast lookup assignment docs
    match /deviceAssignments/{deviceId} {
      allow read: if isSignedIn();
      allow write: if isPlatformAdmin();
    }

    match /hostVenueAssignments/{assignmentId} {
      allow read: if isSignedIn();
      allow write: if isPlatformAdmin();
    }
  }
}
```

## Firebase Storage Rules Draft

```javascript
rules_version = '2';
service firebase.storage {
  match /b/{bucket}/o {

    function isSignedIn() {
      return request.auth != null;
    }

    function isPlatformAdmin() {
      return isSignedIn() && request.auth.token.platformAdmin == true;
    }

    function isCompanyMember(companyId) {
      return isSignedIn()
        && firestore.exists(/databases/(default)/documents/companies/$(companyId)/members/$(request.auth.uid));
    }

    function companyRole(companyId) {
      return firestore.get(/databases/(default)/documents/companies/$(companyId)/members/$(request.auth.uid)).data.role;
    }

    function isCompanyAdmin(companyId) {
      return isCompanyMember(companyId) && companyRole(companyId) == 'company_admin';
    }

    // Song and manifest distribution
    match /companies/{companyId}/{allPaths=**} {
      allow read: if isPlatformAdmin() || isCompanyMember(companyId);
      allow write: if isPlatformAdmin() || isCompanyAdmin(companyId);
    }
  }
}
```

## Hardening Checklist
1. Add App Check enforcement for callable and HTTPS function endpoints.
2. Avoid wildcard client writes to package and campaign docs.
3. Keep publish and fan-out logic in Cloud Functions only.
4. Validate companyId on both auth context and document payload.
5. Use immutable audit records for privileged actions.
6. Add rate limits for status updates per device.

## Integration Notes
- This is a draft. Merge with existing project rules carefully.
- Test rules in Firebase Emulator Suite before production deploy.
- Prefer custom claims for coarse role and Firestore membership docs for tenant scoping.
- Legacy standalone users keep using the top-level hosts/, venues/, user-venue-lookup/, venueInvitations/, and venueJoinRequests/ collections.
- Company users use the companies/{companyId}/... subtree; company membership is optional, not required for basic sign-in.
