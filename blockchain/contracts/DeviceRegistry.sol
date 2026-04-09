// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

import "@openzeppelin/contracts/utils/cryptography/ECDSA.sol";

/**
 * @title DeviceRegistry
 * @notice BCI device authentication and management
 * @dev Implements device registration, verification, and ownership transfer
 */
contract DeviceRegistry {
    using ECDSA for bytes32;

    struct DeviceRecord {
        address deviceId;       // Derived from device public key
        bytes publicKey;        // Device's secp256k1 public key (64 bytes)
        address owner;
        string model;
        string firmwareVersion;
        uint256 registeredAt;
        bool active;
        bool exists;
    }

    mapping(address => DeviceRecord) public devices;
    mapping(address => address[]) public ownerDevices;

    // Nonces for replay protection
    mapping(address => uint256) public deviceNonces;

    event DeviceRegistered(
        address indexed deviceId,
        address indexed owner,
        string model,
        string firmwareVersion,
        uint256 timestamp
    );

    event DeviceRevoked(
        address indexed deviceId,
        address indexed owner,
        uint256 timestamp
    );

    event DeviceReactivated(
        address indexed deviceId,
        address indexed owner,
        uint256 timestamp
    );

    event DeviceTransferred(
        address indexed deviceId,
        address indexed previousOwner,
        address indexed newOwner,
        uint256 timestamp
    );

    event FirmwareUpdated(
        address indexed deviceId,
        string previousVersion,
        string newVersion,
        uint256 timestamp
    );

    modifier onlyDeviceOwner(address deviceId) {
        require(devices[deviceId].exists, "Device not found");
        require(devices[deviceId].owner == msg.sender, "Not device owner");
        _;
    }

    modifier deviceExists(address deviceId) {
        require(devices[deviceId].exists, "Device not found");
        _;
    }

    /**
     * @notice Register a new BCI device
     * @param publicKey Device's secp256k1 public key (64 bytes, uncompressed without prefix)
     * @param model Device model name
     * @param firmwareVersion Initial firmware version
     */
    function registerDevice(
        bytes calldata publicKey,
        string calldata model,
        string calldata firmwareVersion
    ) external returns (address deviceId) {
        require(publicKey.length == 64, "Invalid public key length");
        require(bytes(model).length > 0, "Model required");

        // Derive device address from public key (Ethereum-style)
        deviceId = address(uint160(uint256(keccak256(publicKey))));

        require(!devices[deviceId].exists, "Device already registered");

        devices[deviceId] = DeviceRecord({
            deviceId: deviceId,
            publicKey: publicKey,
            owner: msg.sender,
            model: model,
            firmwareVersion: firmwareVersion,
            registeredAt: block.timestamp,
            active: true,
            exists: true
        });

        ownerDevices[msg.sender].push(deviceId);

        emit DeviceRegistered(deviceId, msg.sender, model, firmwareVersion, block.timestamp);

        return deviceId;
    }

    /**
     * @notice Verify a device is registered and active
     */
    function verifyDevice(address deviceId) external view returns (bool) {
        return devices[deviceId].exists && devices[deviceId].active;
    }

    /**
     * @notice Authenticate device with signature challenge
     * @param deviceId Device address
     * @param challenge Random challenge bytes
     * @param signature Device's signature of keccak256(challenge + nonce)
     */
    function authenticateDevice(
        address deviceId,
        bytes calldata challenge,
        bytes calldata signature
    ) external deviceExists(deviceId) returns (bool) {
        require(devices[deviceId].active, "Device not active");

        // Create message hash with nonce for replay protection
        uint256 nonce = deviceNonces[deviceId];
        bytes32 messageHash = keccak256(abi.encodePacked(challenge, nonce));
        bytes32 ethSignedHash = messageHash.toEthSignedMessageHash();

        // Recover signer from signature
        address recoveredAddr = ethSignedHash.recover(signature);

        if (recoveredAddr == deviceId) {
            deviceNonces[deviceId]++;
            return true;
        }

        return false;
    }

    /**
     * @notice Get device record
     */
    function getDevice(address deviceId)
        external
        view
        deviceExists(deviceId)
        returns (DeviceRecord memory)
    {
        return devices[deviceId];
    }

    /**
     * @notice Update device firmware version
     */
    function updateFirmware(
        address deviceId,
        string calldata newVersion
    ) external onlyDeviceOwner(deviceId) {
        require(bytes(newVersion).length > 0, "Version required");

        string memory previousVersion = devices[deviceId].firmwareVersion;
        devices[deviceId].firmwareVersion = newVersion;

        emit FirmwareUpdated(deviceId, previousVersion, newVersion, block.timestamp);
    }

    /**
     * @notice Revoke/deactivate a device
     */
    function revokeDevice(address deviceId) external onlyDeviceOwner(deviceId) {
        require(devices[deviceId].active, "Device already inactive");

        devices[deviceId].active = false;

        emit DeviceRevoked(deviceId, msg.sender, block.timestamp);
    }

    /**
     * @notice Reactivate a previously revoked device
     */
    function reactivateDevice(address deviceId) external onlyDeviceOwner(deviceId) {
        require(!devices[deviceId].active, "Device already active");

        devices[deviceId].active = true;

        emit DeviceReactivated(deviceId, msg.sender, block.timestamp);
    }

    /**
     * @notice Transfer device ownership
     */
    function transferDevice(
        address deviceId,
        address newOwner
    ) external onlyDeviceOwner(deviceId) {
        require(newOwner != address(0), "Invalid new owner");
        require(newOwner != msg.sender, "Already owner");

        address previousOwner = devices[deviceId].owner;
        devices[deviceId].owner = newOwner;

        // Update owner mappings
        ownerDevices[newOwner].push(deviceId);
        _removeFromOwnerDevices(previousOwner, deviceId);

        emit DeviceTransferred(deviceId, previousOwner, newOwner, block.timestamp);
    }

    /**
     * @notice Get all devices owned by an address
     */
    function getOwnerDevices(address owner)
        external
        view
        returns (address[] memory)
    {
        return ownerDevices[owner];
    }

    /**
     * @notice Get current nonce for a device (for challenge-response)
     */
    function getDeviceNonce(address deviceId) external view returns (uint256) {
        return deviceNonces[deviceId];
    }

    function _removeFromOwnerDevices(address owner, address deviceId) internal {
        address[] storage devs = ownerDevices[owner];
        for (uint256 i = 0; i < devs.length; i++) {
            if (devs[i] == deviceId) {
                devs[i] = devs[devs.length - 1];
                devs.pop();
                break;
            }
        }
    }
}
