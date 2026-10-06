//! Bounded DNS-SD discovery. Advertisements are untrusted hints, never grants.
pub use super::network_types::DiscoveredHost;
use super::{network, Error};
use std::{
    collections::BTreeMap,
    time::{Duration, Instant},
};
pub const SERVICE_TYPE: &str = "_yvex-management._tcp.local.";
pub fn browse(duration: Duration) -> Result<Vec<DiscoveredHost>, Error> {
    if duration.is_zero() || duration > Duration::from_secs(10) {
        return Err(Error::local("invalid_discovery_duration"));
    }
    let daemon =
        mdns_sd::ServiceDaemon::new().map_err(|_| Error::local("discovery_unavailable"))?;
    let events = match daemon.browse(SERVICE_TYPE) {
        Ok(events) => events,
        Err(_) => {
            let _ = daemon.shutdown();
            return Err(Error::local("discovery_unavailable"));
        }
    };
    let deadline = Instant::now() + duration;
    let mut hosts = BTreeMap::new();
    while Instant::now() < deadline {
        match events.recv_timeout(deadline.saturating_duration_since(Instant::now())) {
            Ok(mdns_sd::ServiceEvent::ServiceResolved(service)) => {
                if service.get_property_val_str("protocol") != Some("yvex.management.v2")
                    || service.get_port() == 0
                {
                    continue;
                }
                let label = service
                    .get_property_val_str("display_name")
                    .unwrap_or("YVEX");
                if label.len() > 256
                    || label.chars().any(char::is_control)
                    || service.get_fullname().len() > 512
                {
                    continue;
                }
                let advertised = service
                    .get_property_val_str("device_identity")
                    .filter(|s| network::pin(s).is_ok())
                    .map(str::to_string);
                for address in service.get_addresses().iter().take(16) {
                    let ip = address.to_ip_addr();
                    if ip.is_unspecified() || ip.is_multicast() {
                        continue;
                    }
                    // Link-local IPv6 needs interface scope; never drop that scope
                    // and manufacture a reachable endpoint from an address alone.
                    if matches!(ip,std::net::IpAddr::V6(v) if v.is_unicast_link_local()) {
                        continue;
                    }
                    let endpoint = match ip {
                        std::net::IpAddr::V4(v) => format!("https://{v}:{}", service.get_port()),
                        std::net::IpAddr::V6(v) => format!("https://[{v}]:{}", service.get_port()),
                    };
                    if hosts.len() >= 64 && !hosts.contains_key(&endpoint) {
                        break;
                    }
                    hosts.insert(
                        endpoint.clone(),
                        DiscoveredHost {
                            service_name: service.get_fullname().into(),
                            endpoint,
                            display_name: label.into(),
                            advertised_device_identity: advertised.clone(),
                            trusted: false,
                        },
                    );
                }
            }
            Ok(mdns_sd::ServiceEvent::ServiceRemoved(_, name)) => {
                hosts.retain(|_, host| host.service_name != name)
            }
            Ok(_) => {}
            Err(_) => break,
        }
    }
    let _ = daemon.stop_browse(SERVICE_TYPE);
    let _ = daemon.shutdown();
    Ok(hosts.into_values().collect())
}
