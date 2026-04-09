// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/**
 * @title NeuralDataRegistry
 * @notice Immutable registry for neural data provenance
 * @dev Records data hashes, not actual neural data (privacy by design)
 */
contract NeuralDataRegistry {

    enum DataType { RawSignal, Features, Intent, Model, Feedback }

    struct DataRecord {
        bytes32 dataHash;
        DataType dataType;
        uint256 timestamp;
        address owner;
        address device;
        uint32 size;
        string metadata;
        bool exists;
    }

    struct AuditEntry {
        bytes32 dataHash;
        address actor;
        string action;
        string details;
        uint256 timestamp;
    }

    mapping(bytes32 => DataRecord) public dataRecords;
    mapping(bytes32 => AuditEntry[]) public auditTrails;
    mapping(address => bytes32[]) public userDataHashes;

    event DataRecorded(
        bytes32 indexed dataHash,
        address indexed owner,
        address indexed device,
        DataType dataType,
        uint256 timestamp
    );

    event DataAccessed(
        bytes32 indexed dataHash,
        address indexed accessor,
        string purpose,
        uint256 timestamp
    );

    event DataShared(
        bytes32 indexed dataHash,
        address indexed sharedWith,
        string terms,
        uint256 timestamp
    );

    event DataDeleted(
        bytes32 indexed dataHash,
        address indexed owner,
        string reason,
        uint256 timestamp
    );

    modifier onlyOwner(bytes32 dataHash) {
        require(dataRecords[dataHash].exists, "Data not found");
        require(dataRecords[dataHash].owner == msg.sender, "Not data owner");
        _;
    }

    /**
     * @notice Record neural data hash on-chain
     * @param dataHash Keccak256 hash of the actual data
     * @param dataType Type of neural data
     * @param device Address of the BCI device that captured the data
     * @param size Size of the actual data in bytes
     * @param metadata Optional metadata string
     */
    function recordData(
        bytes32 dataHash,
        DataType dataType,
        address device,
        uint32 size,
        string calldata metadata
    ) external {
        require(!dataRecords[dataHash].exists, "Data already recorded");
        require(dataHash != bytes32(0), "Invalid hash");

        dataRecords[dataHash] = DataRecord({
            dataHash: dataHash,
            dataType: dataType,
            timestamp: block.timestamp,
            owner: msg.sender,
            device: device,
            size: size,
            metadata: metadata,
            exists: true
        });

        userDataHashes[msg.sender].push(dataHash);

        _addAuditEntry(dataHash, msg.sender, "RECORD", "Data recorded");

        emit DataRecorded(dataHash, msg.sender, device, dataType, block.timestamp);
    }

    /**
     * @notice Log data access event
     */
    function logAccess(
        bytes32 dataHash,
        string calldata purpose
    ) external {
        require(dataRecords[dataHash].exists, "Data not found");

        _addAuditEntry(dataHash, msg.sender, "ACCESS", purpose);

        emit DataAccessed(dataHash, msg.sender, purpose, block.timestamp);
    }

    /**
     * @notice Log data sharing event
     */
    function logShare(
        bytes32 dataHash,
        address recipient,
        string calldata terms
    ) external onlyOwner(dataHash) {
        _addAuditEntry(dataHash, msg.sender, "SHARE", terms);

        emit DataShared(dataHash, recipient, terms, block.timestamp);
    }

    /**
     * @notice Log data deletion (data hash remains for audit, marked deleted)
     */
    function logDeletion(
        bytes32 dataHash,
        string calldata reason
    ) external onlyOwner(dataHash) {
        _addAuditEntry(dataHash, msg.sender, "DELETE", reason);

        emit DataDeleted(dataHash, msg.sender, reason, block.timestamp);
    }

    /**
     * @notice Get data record
     */
    function getDataRecord(bytes32 dataHash)
        external
        view
        returns (DataRecord memory)
    {
        require(dataRecords[dataHash].exists, "Data not found");
        return dataRecords[dataHash];
    }

    /**
     * @notice Get audit trail for data
     */
    function getAuditTrail(bytes32 dataHash)
        external
        view
        returns (AuditEntry[] memory)
    {
        return auditTrails[dataHash];
    }

    /**
     * @notice Get all data hashes for a user
     */
    function getUserDataHashes(address user)
        external
        view
        returns (bytes32[] memory)
    {
        return userDataHashes[user];
    }

    function _addAuditEntry(
        bytes32 dataHash,
        address actor,
        string memory action,
        string memory details
    ) internal {
        auditTrails[dataHash].push(AuditEntry({
            dataHash: dataHash,
            actor: actor,
            action: action,
            details: details,
            timestamp: block.timestamp
        }));
    }
}
