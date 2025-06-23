rule EICAR_Test_Rule
{
    meta:
        description = "Detects EICAR test string"
        author = "Manlab"
        severity = "low"

    strings:
        $eicar = "X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*"

    condition:
        $eicar
}