# Distributed TLS credential compatibility

Distribution-manager connections now require equivalent symmetric 256-bit
strength by default. Existing credential stores are retained unchanged; weaker
stored keys or certificate-chain signatures will fail the configured minimum.

Applications with older credentials can replace those credentials and update
peer trust, or explicitly select a lower minimum with
`CDistributedAppActor_Settings::f_MinimumCryptoStrength` or the trust manager's
`m_MinimumCryptoStrength` option. `mc_Compatible` retains stronger algorithm
preferences without enforcing a minimum. Lowering the setting permits weaker
connections and should be an explicit application decision.
