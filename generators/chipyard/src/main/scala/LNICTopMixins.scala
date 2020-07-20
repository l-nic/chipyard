package chipyard

import chisel3._

import freechips.rocketchip.config._
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.util.PlusArg
import lnic._

/** Top-level mixins for including LNIC **/

trait CanHaveLNIC { this: Subsystem =>
  val lnicTiles = tiles
  val lnicOpt = p(LNICKey).map { params =>
    val lnic = LazyModule(new LNIC)
    lnic
  }
}

trait CanHaveLNICModuleImp extends LazyModuleImp {
  val outer: CanHaveLNIC

  val net = outer.lnicOpt.map { lnic =>
    // create and connect to top-level IO
    val nicio = IO(new NICIOvonly)
    val vonly = NICIOvonly(lnic.module.io.net)
    nicio.out <> vonly.out
    vonly.in <> nicio.in
    vonly.nic_mac_addr := nicio.nic_mac_addr
    vonly.switch_mac_addr := nicio.switch_mac_addr
    vonly.nic_ip_addr := nicio.nic_ip_addr
    vonly.timeout_cycles := nicio.timeout_cycles
    vonly.rtt_pkts := nicio.rtt_pkts
    // connect L-NIC to tiles
    val num_cores = outer.lnicTiles.size
    for (i <- 0 until num_cores) {
      val tile = outer.lnicTiles(i)
      lnic.module.io.core(i).net_in <> tile.module.net.get.net_out
      lnic.module.io.core(i).add_context := tile.module.net.get.add_context
      lnic.module.io.core(i).get_next_msg := tile.module.net.get.get_next_msg
      tile.module.net.get.reset_done := lnic.module.io.core(i).reset_done
      tile.module.net.get.net_in <> lnic.module.io.core(i).net_out
      tile.module.net.get.meta_in := lnic.module.io.core(i).meta_out
    }
    nicio
  }

  // Connect L-NIC to simulated network.
  def connectSimNetwork(clock: Clock, reset: Bool) {
    val sim = Module(new SimNetwork)
    val latency = Module(new LatencyModule)
    sim.io.clock := clock
    sim.io.reset := reset

    sim.io.net.out <> latency.io.net.out
    latency.io.net.in <> sim.io.net.in

    latency.io.nic.in <> net.get.out
    net.get.in <> latency.io.nic.out

    // plus args used inside SimNetwork module
    val nic_mac_addr = PlusArg("nic_mac_addr",
                               docstring = "MAC address of the L-NIC",
                               width = 64)
    val switch_mac_addr = PlusArg("switch_mac_addr",
                               docstring = "MAC address of port on the upstream switch",
                               width = 64)
    val nic_ip_addr = PlusArg("nic_ip_addr",
                             docstring = "IP address of the L-NIC",
                             width = 32)
    val timeout_cycles = PlusArg("timeout_cycles",
                                docstring = "Retransmission timeout in cycles",
                                width = 64)
    val rtt_pkts = PlusArg("rtt_pkts",
                           docstring = "Number of outstanding packets",
                           width = 16)

    net.get.nic_mac_addr := sim.io.net.nic_mac_addr
    net.get.switch_mac_addr := sim.io.net.switch_mac_addr
    net.get.nic_ip_addr := sim.io.net.nic_ip_addr
    net.get.timeout_cycles := sim.io.net.timeout_cycles
    net.get.rtt_pkts := sim.io.net.rtt_pkts
  }

}

