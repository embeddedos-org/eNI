// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/**
 * @title ConsentManager
 * @notice Smart contract-based consent management for neural data
 * @dev Implements granular permission control with time-based expiration
 */
contract ConsentManager {

    // Consent flags (bitwise)
    uint8 public constant CONSENT_COLLECT    = 1 << 0;  // 0x01
    uint8 public constant CONSENT_STORE      = 1 << 1;  // 0x02
    uint8 public constant CONSENT_PROCESS    = 1 << 2;  // 0x04
    uint8 public constant CONSENT_SHARE      = 1 << 3;  // 0x08
    uint8 public constant CONSENT_RESEARCH   = 1 << 4;  // 0x10
    uint8 public constant CONSENT_COMMERCIAL = 1 << 5;  // 0x20
    uint8 public constant CONSENT_ALL        = 0x3F;

    struct ConsentRecord {
        address user;
        address grantee;
        uint8 flags;
        uint256 grantedAt;
        uint256 expiresAt;
        bool revoked;
        bool exists;
    }

    // user => grantee => consent
    mapping(address => mapping(address => ConsentRecord)) public consents;

    // user => list of grantees
    mapping(address => address[]) public userGrantees;

    // grantee => list of users who granted consent
    mapping(address => address[]) public granteeUsers;

    event ConsentGranted(
        address indexed user,
        address indexed grantee,
        uint8 flags,
        uint256 expiresAt,
        uint256 timestamp
    );

    event ConsentRevoked(
        address indexed user,
        address indexed grantee,
        uint256 timestamp
    );

    event ConsentUpdated(
        address indexed user,
        address indexed grantee,
        uint8 newFlags,
        uint256 timestamp
    );

    event ConsentExtended(
        address indexed user,
        address indexed grantee,
        uint256 newExpiresAt,
        uint256 timestamp
    );

    event AllConsentsRevoked(
        address indexed user,
        uint256 timestamp
    );

    modifier onlyUser(address user) {
        require(msg.sender == user, "Not authorized");
        _;
    }

    /**
     * @notice Grant consent to a grantee
     * @param grantee Address receiving consent
     * @param flags Bitwise consent flags
     * @param duration Duration in seconds (0 = indefinite)
     */
    function grantConsent(
        address grantee,
        uint8 flags,
        uint256 duration
    ) external {
        require(grantee != address(0), "Invalid grantee");
        require(grantee != msg.sender, "Cannot grant to self");
        require(flags > 0 && flags <= CONSENT_ALL, "Invalid flags");

        uint256 expiresAt = duration > 0 ? block.timestamp + duration : 0;

        if (!consents[msg.sender][grantee].exists) {
            userGrantees[msg.sender].push(grantee);
            granteeUsers[grantee].push(msg.sender);
        }

        consents[msg.sender][grantee] = ConsentRecord({
            user: msg.sender,
            grantee: grantee,
            flags: flags,
            grantedAt: block.timestamp,
            expiresAt: expiresAt,
            revoked: false,
            exists: true
        });

        emit ConsentGranted(msg.sender, grantee, flags, expiresAt, block.timestamp);
    }

    /**
     * @notice Revoke consent from a grantee
     */
    function revokeConsent(address grantee) external {
        require(consents[msg.sender][grantee].exists, "Consent not found");
        require(!consents[msg.sender][grantee].revoked, "Already revoked");

        consents[msg.sender][grantee].revoked = true;

        emit ConsentRevoked(msg.sender, grantee, block.timestamp);
    }

    /**
     * @notice Revoke all consents granted by the caller
     */
    function revokeAllConsents() external {
        address[] storage grantees = userGrantees[msg.sender];

        for (uint256 i = 0; i < grantees.length; i++) {
            if (!consents[msg.sender][grantees[i]].revoked) {
                consents[msg.sender][grantees[i]].revoked = true;
            }
        }

        emit AllConsentsRevoked(msg.sender, block.timestamp);
    }

    /**
     * @notice Update consent flags
     */
    function updateConsent(address grantee, uint8 newFlags) external {
        require(consents[msg.sender][grantee].exists, "Consent not found");
        require(!consents[msg.sender][grantee].revoked, "Consent revoked");
        require(newFlags > 0 && newFlags <= CONSENT_ALL, "Invalid flags");

        consents[msg.sender][grantee].flags = newFlags;

        emit ConsentUpdated(msg.sender, grantee, newFlags, block.timestamp);
    }

    /**
     * @notice Extend consent expiration
     */
    function extendConsent(address grantee, uint256 additionalDuration) external {
        require(consents[msg.sender][grantee].exists, "Consent not found");
        require(!consents[msg.sender][grantee].revoked, "Consent revoked");
        require(additionalDuration > 0, "Duration must be positive");

        ConsentRecord storage consent = consents[msg.sender][grantee];

        if (consent.expiresAt == 0) {
            // Was indefinite, now has expiration
            consent.expiresAt = block.timestamp + additionalDuration;
        } else {
            consent.expiresAt += additionalDuration;
        }

        emit ConsentExtended(msg.sender, grantee, consent.expiresAt, block.timestamp);
    }

    /**
     * @notice Check if consent is granted for specific flags
     * @param user Data owner
     * @param grantee Entity requesting access
     * @param requiredFlags Flags to check
     * @return granted True if all required flags are granted and not expired
     */
    function checkConsent(
        address user,
        address grantee,
        uint8 requiredFlags
    ) external view returns (bool granted) {
        ConsentRecord storage consent = consents[user][grantee];

        if (!consent.exists || consent.revoked) {
            return false;
        }

        // Check expiration
        if (consent.expiresAt > 0 && block.timestamp > consent.expiresAt) {
            return false;
        }

        // Check if all required flags are set
        return (consent.flags & requiredFlags) == requiredFlags;
    }

    /**
     * @notice Get consent record
     */
    function getConsent(
        address user,
        address grantee
    ) external view returns (ConsentRecord memory) {
        return consents[user][grantee];
    }

    /**
     * @notice Get all grantees for a user
     */
    function getUserGrantees(address user)
        external
        view
        returns (address[] memory)
    {
        return userGrantees[user];
    }

    /**
     * @notice Get all users who granted consent to a grantee
     */
    function getGranteeUsers(address grantee)
        external
        view
        returns (address[] memory)
    {
        return granteeUsers[grantee];
    }

    /**
     * @notice Check if consent is currently valid (not revoked, not expired)
     */
    function isConsentValid(
        address user,
        address grantee
    ) external view returns (bool) {
        ConsentRecord storage consent = consents[user][grantee];

        if (!consent.exists || consent.revoked) {
            return false;
        }

        if (consent.expiresAt > 0 && block.timestamp > consent.expiresAt) {
            return false;
        }

        return true;
    }
}
